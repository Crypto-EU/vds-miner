/** Same-origin so the portable app (and Vite) can proxy CORS-blocked POSTs. */
const EXPLORER = "/vds-api";
const EXPLORER_TX = "https://vdscool.com/tx/";
const POOL = "/pool-api";

async function getJson<T>(url: string): Promise<T> {
  const r = await fetch(url);
  if (!r.ok) throw new Error(`HTTP ${r.status} für ${url}`);
  return (await r.json()) as T;
}

export type ChainInfo = {
  height: number;
  feeRate: number;
};

export type AddressInfo = {
  addrstr: string;
  balance: string;
  unconfirmed: string;
  totalReceived: string;
  totalSent: string;
  transactionCount: number;
};

export type UtxoInfo = {
  transactionId: string;
  outputIndex: number;
  scriptPubKey: string;
  address: string;
  value: string;
  confirmations: number;
  isStake?: boolean;
};

export type AddressTxs = {
  totalCount: number;
  transactions: string[];
};

export type PoolWallet = {
  nowHash: number;
  dayHash: string;
  balance: number;
  noBalance: number;
  yesBalance: number;
  lastPay: number;
  allPay: number;
  dayIncome: number;
  onWorks: number | null;
  offWorks: number | null;
  allWorks: number | null;
  worksOnline: { name?: string; hash?: number; status?: string }[];
  payData: { amount?: number; txid?: string; time?: string }[];
  wallet: string;
  coin: string;
};

export async function fetchChainInfo(): Promise<ChainInfo> {
  return getJson<ChainInfo>(`${EXPLORER}/info`);
}

export async function fetchAddress(addr: string): Promise<AddressInfo> {
  return getJson<AddressInfo>(`${EXPLORER}/address/${encodeURIComponent(addr)}`);
}

export async function fetchUtxos(addr: string): Promise<UtxoInfo[]> {
  const data = await getJson<UtxoInfo[]>(`${EXPLORER}/address/${encodeURIComponent(addr)}/utxo`);
  return Array.isArray(data) ? data : [];
}

export async function fetchAddressTxs(addr: string): Promise<AddressTxs> {
  return getJson<AddressTxs>(`${EXPLORER}/address/${encodeURIComponent(addr)}/txs`);
}

export async function fetchPoolWallet(addr: string): Promise<PoolWallet> {
  const url = `${POOL}/server/v1/getWalletData/?coin=VDS&wallet=${encodeURIComponent(addr)}&authKey=`;
  const j = await getJson<{ retCode: number; retMessage: string; data: PoolWallet }>(url);
  if (j.retCode !== 200) throw new Error(j.retMessage || "Pool-Antwort ungültig");
  return j.data;
}

export type BroadcastResult = { status: number; message?: string; txid?: string; id?: string; hash?: string };

/** POST via Vite proxy — explorer CORS preflight rejects JSON from the browser. */
export async function broadcastRaw(hex: string): Promise<BroadcastResult> {
  const r = await fetch("/vds-api/tx/send", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ rawtx: hex }),
  });
  const text = await r.text();
  let j: BroadcastResult;
  try {
    j = JSON.parse(text) as BroadcastResult;
  } catch {
    throw new Error("Broadcast-Antwort unlesbar: " + text.slice(0, 180));
  }
  if (j.status && j.status !== 0) {
    throw new Error(j.message || "Senden abgelehnt");
  }
  return j;
}

export function explorerTxUrl(id: string): string {
  return EXPLORER_TX + id;
}

export { EXPLORER_TX };
