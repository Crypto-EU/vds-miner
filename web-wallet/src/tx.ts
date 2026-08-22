import { sha256 } from "@noble/hashes/sha2.js";
import * as secp from "@noble/secp256k1";
import { hmac } from "@noble/hashes/hmac.js";
import {
  bytesToHex,
  compactSize,
  concat,
  displayHexToInternal,
  i32le,
  i64le,
  u32le,
  withCompact,
} from "./bytes";
import { p2pkhScriptFromAddress, pubkeyFromSecret, secretFromWif } from "./vds";
import { DEFAULT_FEE_SATS } from "./amount";

secp.hashes.hmacSha256 = (key, msg) => hmac(sha256, key, msg);
secp.hashes.sha256 = sha256;

const SIGHASH_ALL = 1;
const SEQUENCE_FINAL = 0xffffffff;
const ZERO32 = new Uint8Array(32);
const ZERO64 = new Uint8Array(64);

export type TxInput = {
  prevTxId: string;
  outputIndex: number;
  scriptSig: Uint8Array;
  sequence: number;
  /** Previous output script, used as scriptCode when signing. */
  scriptPubKey: Uint8Array;
  value: bigint;
};

export type TxOutput = {
  value: bigint;
  nFlag: number;
  scriptPubKey: Uint8Array;
  dataHash: Uint8Array;
};

export type VdsTx = {
  version: number;
  nFlag: number;
  vin: TxInput[];
  vout: TxOutput[];
  lockTime: number;
  expiryHeight: number;
  valueBalance: bigint;
};

function sha256d(data: Uint8Array): Uint8Array {
  return sha256(sha256(data));
}

function serOutPoint(prevTxId: string, outputIndex: number): Uint8Array {
  return concat(displayHexToInternal(prevTxId), u32le(outputIndex));
}

function serInput(inp: TxInput): Uint8Array {
  return concat(serOutPoint(inp.prevTxId, inp.outputIndex), withCompact(inp.scriptSig), u32le(inp.sequence));
}

function serOutput(out: TxOutput): Uint8Array {
  return concat(i64le(out.value), new Uint8Array([out.nFlag & 0xff]), withCompact(out.scriptPubKey), out.dataHash);
}

/** Full network serialization. Decoder-checked against live vdscool `/tx/send`. */
export function serializeTx(tx: VdsTx): Uint8Array {
  const parts: Uint8Array[] = [i32le(tx.version), new Uint8Array([tx.nFlag & 0xff]), compactSize(tx.vin.length)];
  for (const inp of tx.vin) parts.push(serInput(inp));
  parts.push(compactSize(tx.vout.length));
  for (const out of tx.vout) parts.push(serOutput(out));
  for (let i = 0; i < tx.vin.length; i++) parts.push(compactSize(0)); // empty scriptWitness
  parts.push(u32le(tx.lockTime));
  parts.push(u32le(tx.expiryHeight));
  parts.push(i64le(tx.valueBalance));
  parts.push(compactSize(0), compactSize(0)); // shielded spend/output
  parts.push(ZERO64); // bindingSig
  return concat(...parts);
}

export function txidDisplay(raw: Uint8Array): string {
  return bytesToHex(sha256d(raw).reverse());
}

function hashPrevouts(tx: VdsTx): Uint8Array {
  const chunks = tx.vin.map((inp) => serOutPoint(inp.prevTxId, inp.outputIndex));
  return sha256d(concat(...chunks));
}

function hashSequence(tx: VdsTx): Uint8Array {
  const chunks = tx.vin.map((inp) => u32le(inp.sequence));
  return sha256d(concat(...chunks));
}

function hashOutputs(tx: VdsTx): Uint8Array {
  const chunks = tx.vout.map(serOutput);
  return sha256d(concat(...chunks));
}

/** VDS SignatureHash for a transparent input (vds-core interpreter.cpp). */
export function signatureHash(tx: VdsTx, nIn: number, scriptCode: Uint8Array, nHashType = SIGHASH_ALL): Uint8Array {
  if (nIn >= tx.vin.length) throw new Error("Eingabe-Index außerhalb");
  const ss = concat(
    hashPrevouts(tx),
    hashSequence(tx),
    hashOutputs(tx),
    ZERO32, // hashShieldedSpends (empty)
    ZERO32, // hashShieldedOutputs (empty)
    u32le(tx.lockTime),
    u32le(tx.expiryHeight),
    i64le(tx.valueBalance),
    i32le(nHashType),
    serOutPoint(tx.vin[nIn].prevTxId, tx.vin[nIn].outputIndex),
    withCompact(scriptCode),
    u32le(tx.vin[nIn].sequence),
  );
  return sha256d(ss);
}

function pushScript(sigDer: Uint8Array, pub: Uint8Array): Uint8Array {
  const sigWithType = concat(sigDer, new Uint8Array([SIGHASH_ALL]));
  return concat(withCompact(sigWithType), withCompact(pub));
}

export type UtxoPick = {
  transactionId: string;
  outputIndex: number;
  scriptPubKey: string;
  value: bigint;
};

export type BuiltSend = {
  hex: string;
  txid: string;
  fee: bigint;
  change: bigint;
  size: number;
};

export function selectUtxos(utxos: UtxoPick[], amount: bigint, fee: bigint): UtxoPick[] {
  const need = amount + fee;
  const sorted = [...utxos].filter((u) => u.value > 0n).sort((a, b) => (a.value < b.value ? -1 : a.value > b.value ? 1 : 0));
  const picked: UtxoPick[] = [];
  let total = 0n;
  for (const u of sorted) {
    picked.push(u);
    total += u.value;
    if (total >= need) return picked;
  }
  throw new Error("Zu wenig Guthaben für Betrag plus Gebühr");
}

export function buildSignedSend(opts: {
  wif: string;
  fromAddress: string;
  toAddress: string;
  amount: bigint;
  utxos: UtxoPick[];
  fee?: bigint;
}): BuiltSend {
  const fee = opts.fee ?? DEFAULT_FEE_SATS;
  if (opts.amount <= 0n) throw new Error("Betrag muss größer als 0 sein");
  if (fee < 0n) throw new Error("Gebühr ungültig");
  const secret = secretFromWif(opts.wif);
  const pub = pubkeyFromSecret(secret);
  const picked = selectUtxos(opts.utxos, opts.amount, fee);
  const totalIn = picked.reduce((a, u) => a + u.value, 0n);
  const change = totalIn - opts.amount - fee;
  if (change < 0n) throw new Error("Zu wenig Guthaben");

  const vout: TxOutput[] = [
    {
      value: opts.amount,
      nFlag: 0,
      scriptPubKey: p2pkhScriptFromAddress(opts.toAddress),
      dataHash: ZERO32,
    },
  ];
  if (change > 546n) {
    vout.push({
      value: change,
      nFlag: 0,
      scriptPubKey: p2pkhScriptFromAddress(opts.fromAddress),
      dataHash: ZERO32,
    });
  } else if (change > 0n && change <= 546n) {
    throw new Error("Wechselgeld wäre Staub — Betrag oder Gebühr anpassen");
  }

  const tx: VdsTx = {
    version: 2,
    nFlag: 0,
    vin: picked.map((u) => ({
      prevTxId: u.transactionId,
      outputIndex: u.outputIndex,
      scriptSig: new Uint8Array(0),
      sequence: SEQUENCE_FINAL,
      scriptPubKey: hexToBytesLocal(u.scriptPubKey),
      value: u.value,
    })),
    vout,
    lockTime: 0,
    expiryHeight: 0,
    valueBalance: 0n,
  };

  for (let i = 0; i < tx.vin.length; i++) {
    const sighash = signatureHash(tx, i, tx.vin[i].scriptPubKey, SIGHASH_ALL);
    // sighash is already sha256d; do not hash again (Bitcoin/VDS ECDSA).
    const der = secp.sign(sighash, secret, { format: "der", prehash: false, lowS: true });
    tx.vin[i].scriptSig = pushScript(der, pub);
  }

  const raw = serializeTx(tx);
  return {
    hex: bytesToHex(raw),
    txid: txidDisplay(raw),
    fee,
    change: change > 546n ? change : 0n,
    size: raw.length,
  };
}

function hexToBytesLocal(hex: string): Uint8Array {
  const h = hex.trim().replace(/^0x/i, "");
  const out = new Uint8Array(h.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = parseInt(h.slice(i * 2, i * 2 + 2), 16);
  return out;
}

export function dummyTxSize(): number {
  const tx: VdsTx = {
    version: 2,
    nFlag: 0,
    vin: [
      {
        prevTxId: "00".repeat(32),
        outputIndex: 0,
        scriptSig: new Uint8Array(106),
        sequence: SEQUENCE_FINAL,
        scriptPubKey: new Uint8Array(25),
        value: 1n,
      },
    ],
    vout: [
      { value: 1n, nFlag: 0, scriptPubKey: new Uint8Array(25), dataHash: ZERO32 },
      { value: 1n, nFlag: 0, scriptPubKey: new Uint8Array(25), dataHash: ZERO32 },
    ],
    lockTime: 0,
    expiryHeight: 0,
    valueBalance: 0n,
  };
  return serializeTx(tx).length;
}
