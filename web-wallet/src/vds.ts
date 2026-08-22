import * as secp from "@noble/secp256k1";
import { hmac } from "@noble/hashes/hmac.js";
import { sha256 } from "@noble/hashes/sha2.js";
import { ripemd160 } from "@noble/hashes/legacy.js";
import { base58 } from "@scure/base";

secp.hashes.hmacSha256 = (key, msg) => hmac(sha256, key, msg);
secp.hashes.sha256 = sha256;

/** VDS mainnet transparent P2PKH, same as vds-core chainparams. */
export const VDS_PUBKEY_PREFIX = new Uint8Array([0x10, 0x1c]);
/** Bitcoin-style WIF version byte used by vds-core SECRET_KEY. */
export const VDS_WIF_PREFIX = 0x80;

export const SAMPLE_VC = "VcUrphFXcmEUKm3MSDqyntF2KKCCKDXyhXo";

export type VdsKeypair = {
  address: string;
  wif: string;
  secretHex: string;
};

function concat(...parts: Uint8Array[]): Uint8Array {
  const n = parts.reduce((a, p) => a + p.length, 0);
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
}

function checksum4(payload: Uint8Array): Uint8Array {
  return sha256(sha256(payload)).slice(0, 4);
}

function base58check(payload: Uint8Array): string {
  return base58.encode(concat(payload, checksum4(payload)));
}

function hash160(data: Uint8Array): Uint8Array {
  return ripemd160(sha256(data));
}

export function decodeBase58Check(addr: string): Uint8Array {
  const raw = base58.decode(addr);
  if (raw.length < 5) throw new Error("Adresse zu kurz");
  const payload = raw.slice(0, -4);
  const sum = raw.slice(-4);
  const expect = checksum4(payload);
  if (sum[0] !== expect[0] || sum[1] !== expect[1] || sum[2] !== expect[2] || sum[3] !== expect[3]) {
    throw new Error("Ungültige Prüfsumme");
  }
  return payload;
}

export function addressFromPubkey(pubCompressed: Uint8Array): string {
  if (pubCompressed.length !== 33) throw new Error("Pubkey muss komprimiert sein (33 Byte)");
  return base58check(concat(VDS_PUBKEY_PREFIX, hash160(pubCompressed)));
}

export function wifFromSecret(secret: Uint8Array): string {
  if (secret.length !== 32) throw new Error("Privater Schlüssel muss 32 Byte sein");
  return base58check(concat(new Uint8Array([VDS_WIF_PREFIX]), secret, new Uint8Array([0x01])));
}

export function secretFromWif(wif: string): Uint8Array {
  const payload = decodeBase58Check(wif.trim());
  if (payload[0] !== VDS_WIF_PREFIX) throw new Error("Kein VDS/Bitcoin-WIF (Version 0x80)");
  if (payload.length === 34 && payload[33] === 0x01) return payload.slice(1, 33);
  if (payload.length === 33) return payload.slice(1);
  throw new Error("Unbekanntes WIF-Format");
}

export function keypairFromSecret(secret: Uint8Array): VdsKeypair {
  const publicKey = secp.getPublicKey(secret, true);
  return {
    address: addressFromPubkey(publicKey),
    wif: wifFromSecret(secret),
    secretHex: BufferHex(secret),
  };
}

function BufferHex(u: Uint8Array): string {
  return Array.from(u)
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");
}

export function generateKeypair(): VdsKeypair {
  const { secretKey } = secp.keygen();
  return keypairFromSecret(secretKey);
}

export function importWif(wif: string): VdsKeypair {
  return keypairFromSecret(secretFromWif(wif));
}

export function isVcAddress(addr: string): boolean {
  try {
    const p = decodeBase58Check(addr.trim());
    return p.length === 22 && p[0] === 0x10 && p[1] === 0x1c;
  } catch {
    return false;
  }
}

/** HASH160 of the compressed pubkey, from a Vc address. */
export function hash160FromAddress(addr: string): Uint8Array {
  const p = decodeBase58Check(addr.trim());
  if (p.length !== 22 || p[0] !== 0x10 || p[1] !== 0x1c) {
    throw new Error("Keine VDS-Adresse (muss mit Vc beginnen)");
  }
  return p.slice(2);
}

/** Standard P2PKH scriptPubKey. */
export function p2pkhScript(h160: Uint8Array): Uint8Array {
  if (h160.length !== 20) throw new Error("HASH160 muss 20 Byte sein");
  return concat(new Uint8Array([0x76, 0xa9, 0x14]), h160, new Uint8Array([0x88, 0xac]));
}

export function pubkeyFromSecret(secret: Uint8Array): Uint8Array {
  return secp.getPublicKey(secret, true);
}

export function p2pkhScriptFromAddress(addr: string): Uint8Array {
  return p2pkhScript(hash160FromAddress(addr));
}

export function hexToBytes(hex: string): Uint8Array {
  const h = hex.trim().replace(/^0x/i, "");
  if (h.length % 2) throw new Error("Ungerade Hex-Länge");
  const out = new Uint8Array(h.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = parseInt(h.slice(i * 2, i * 2 + 2), 16);
  return out;
}

export function bytesToHex(u: Uint8Array): string {
  return Array.from(u)
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");
}

export { concat, hash160, sha256 };

/** Sanity: known mainnet address uses prefix 0x10 0x1c and a valid checksum. */
export function selfTest(): string[] {
  const errors: string[] = [];
  try {
    const p = decodeBase58Check(SAMPLE_VC);
    if (p[0] !== 0x10 || p[1] !== 0x1c) errors.push("Beispiel-Adresse hat nicht Prefix 0x101c");
    if (p.length !== 22) errors.push("Beispiel-Adresse hat falsche Länge");
  } catch (e) {
    errors.push(String(e));
  }
  try {
    const kp = generateKeypair();
    if (!kp.address.startsWith("Vc")) errors.push("Neue Adresse beginnt nicht mit Vc");
    const again = importWif(kp.wif);
    if (again.address !== kp.address) errors.push("WIF-Import ergibt andere Adresse");
  } catch (e) {
    errors.push("Schlüsselerzeugung: " + String(e));
  }
  return errors;
}
