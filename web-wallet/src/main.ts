import "./style.css";
import QRCode from "qrcode";
import {
  generateKeypair,
  importWif,
  isVcAddress,
  selfTest,
  type VdsKeypair,
} from "./vds";
import { formatVds, parseVds, DEFAULT_FEE_SATS } from "./amount";
import { buildSignedSend } from "./tx";
import {
  broadcastRaw,
  explorerTxUrl,
  fetchAddress,
  fetchAddressTxs,
  fetchChainInfo,
  fetchPoolWallet,
  fetchUtxos,
  type PoolWallet,
} from "./chain";

const STORE_SESSION = "vds-wallet-session";
const STORE_LOCAL = "vds-wallet-local";

type Tab = "receive" | "send" | "pool" | "keys";
type Screen = "empty" | "wallet";

let screen: Screen = "empty";
let tab: Tab = "receive";
let wallet: VdsKeypair | null = null;
let persist = false;
let importOpen = false;
let reveal = false;
let errorText = "";
let noticeText = "";

let chainHeight = 0;
let chainFeeRate = 0;
let onChain: { balance: bigint; unconfirmed: bigint; txs: number; received: bigint } | null = null;
let utxoCount = 0;
let history: string[] = [];
let pool: PoolWallet | null = null;
let loading = false;
let loadError = "";
let sendBusy = false;
let sendTo = "";
let sendAmt = "";
let lastTxid = "";

const app = document.querySelector<HTMLDivElement>("#app")!;

function esc(s: string): string {
  return s
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function sats(n: string | number | null | undefined): bigint {
  if (n === null || n === undefined || n === "") return 0n;
  try {
    return BigInt(String(n).split(".")[0]);
  } catch {
    return 0n;
  }
}

function loadStored(): VdsKeypair | null {
  try {
    const raw = sessionStorage.getItem(STORE_SESSION) || localStorage.getItem(STORE_LOCAL);
    if (!raw) return null;
    const j = JSON.parse(raw) as VdsKeypair;
    if (j?.address && j?.wif) return importWif(j.wif);
  } catch {
    /* ignore */
  }
  return null;
}

function saveWallet(kp: VdsKeypair, keep: boolean) {
  const payload = JSON.stringify({ address: kp.address, wif: kp.wif });
  sessionStorage.setItem(STORE_SESSION, payload);
  if (keep) localStorage.setItem(STORE_LOCAL, payload);
  else localStorage.removeItem(STORE_LOCAL);
}

function clearWallet() {
  sessionStorage.removeItem(STORE_SESSION);
  localStorage.removeItem(STORE_LOCAL);
}

async function refresh() {
  if (!wallet) return;
  loading = true;
  loadError = "";
  render();
  try {
    const [info, addr, utxos, txs, poolData] = await Promise.all([
      fetchChainInfo().catch(() => null),
      fetchAddress(wallet.address),
      fetchUtxos(wallet.address),
      fetchAddressTxs(wallet.address).catch(() => ({ totalCount: 0, transactions: [] })),
      fetchPoolWallet(wallet.address).catch(() => null),
    ]);
    if (info) {
      chainHeight = info.height;
      chainFeeRate = info.feeRate;
    }
    onChain = {
      balance: sats(addr.balance),
      unconfirmed: sats(addr.unconfirmed),
      txs: addr.transactionCount ?? 0,
      received: sats(addr.totalReceived),
    };
    utxoCount = utxos.filter((u) => !u.isStake).length;
    history = (txs.transactions ?? []).slice(0, 8);
    pool = poolData;
  } catch (e) {
    loadError = String(e);
  } finally {
    loading = false;
    render();
    void paintQr();
  }
}

async function paintQr() {
  const canvas = document.getElementById("qr") as HTMLCanvasElement | null;
  if (!canvas || !wallet) return;
  try {
    await QRCode.toCanvas(canvas, wallet.address, {
      width: 196,
      margin: 1,
      color: { dark: "#1a140c", light: "#f3ece3" },
    });
  } catch {
    /* canvas optional */
  }
}

function header() {
  const tests = selfTest();
  const testOk = tests.length === 0;
  return `
    <header>
      <p class="eyebrow">VDS · Vollar · 666pool</p>
      <h1>VDS-Wallet</h1>
      <p class="lead">Empfangen und senden auf der echten VDS-Chain (<strong>Vc…</strong>). Nicht MetaMask, nicht der BNB-/Polygon-Token gleichen Namens.</p>
    </header>
    ${
      !testOk
        ? `<div class="banner err" role="alert">Selbsttest fehlgeschlagen: ${esc(tests.join("; "))}</div>`
        : `<div class="banner ok">Adresse, Explorer und Pool sind verdrahtet. Schlüssel bleiben in diesem Browser.</div>`
    }
    ${errorText ? `<div class="banner err" role="alert">${esc(errorText)}</div>` : ""}
    ${noticeText ? `<div class="banner ok">${esc(noticeText)}</div>` : ""}
  `;
}

function emptyScreen() {
  return `
    ${header()}
    <section class="card empty">
      <h2>Wallet öffnen</h2>
      <p>Eine <strong>Vc-Adresse</strong> ist die Empfangsadresse für HiveOS und 666pool. Der private Schlüssel (WIF) wird zum Senden gebraucht.</p>
      <div class="row">
        <button type="button" class="primary" id="btn-gen">Neue Wallet erzeugen</button>
        <button type="button" class="ghost" id="btn-import-toggle">Vorhandenen Schlüssel einfügen</button>
      </div>
      ${
        importOpen
          ? `
        <label class="block">
          WIF
          <textarea id="wif-in" rows="3" placeholder="beginnt typischerweise mit K oder L"></textarea>
        </label>
        <button type="button" class="primary" id="btn-import">Wallet öffnen</button>
      `
          : ""
      }
      <label class="check">
        <input type="checkbox" id="persist" ${persist ? "checked" : ""} />
        Auf diesem Gerät merken (localStorage — nur eigener PC)
      </label>
    </section>
    <section class="foot">
      <h2>Warum core-qt.exe nicht geht</h2>
      <p>Die Windows-Wallet von 2019 schaltet sich nach Block <code>193076</code> ab. Die Chain steht bei über <strong>3,6 Millionen</strong> Blöcken. Diese Seite spricht den lebenden Explorer <a href="https://vdscool.com/" target="_blank" rel="noreferrer">vdscool.com</a> an.</p>
    </section>
  `;
}

function tabs() {
  const item = (id: Tab, label: string) =>
    `<button type="button" class="tab ${tab === id ? "on" : ""}" data-tab="${id}">${label}</button>`;
  return `<nav class="tabs">${item("receive", "Empfangen")}${item("send", "Senden")}${item("pool", "Pool")}${item("keys", "Schlüssel")}</nav>`;
}

function receiveTab() {
  const bal = onChain ? formatVds(onChain.balance) : "…";
  const unc = onChain && onChain.unconfirmed > 0n ? ` + ${formatVds(onChain.unconfirmed)} unbestätigt` : "";
  return `
    <section class="card">
      <h2>Empfangsadresse</h2>
      <p class="hint">Ins HiveOS-Feld <em>Wallet</em> — ohne Worker-Namen. Andere können dir hierher VDS schicken.</p>
      <div class="addr" id="addr">${esc(wallet!.address)}</div>
      <div class="qr-wrap"><canvas id="qr" width="196" height="196"></canvas></div>
      <div class="row">
        <button type="button" class="primary" data-copy="address">Adresse kopieren</button>
        <button type="button" class="ghost" id="btn-download">Backup herunterladen</button>
      </div>
    </section>
    <section class="card">
      <h2>Kontostand on-chain</h2>
      ${loading ? `<p class="hint">Lade Explorer…</p>` : ""}
      ${loadError ? `<p class="err-inline">${esc(loadError)}</p>` : ""}
      <p class="balance">${esc(bal)} <span>VDS</span>${esc(unc)}</p>
      <p class="hint">Blockhöhe ${chainHeight || "—"} · ${utxoCount} UTXO · ${onChain?.txs ?? 0} Transaktionen${
        chainFeeRate ? ` · Gebührensatz ${chainFeeRate.toFixed(2)}` : ""
      }</p>
      ${
        onChain && onChain.balance === 0n && (!pool || Number(pool.balance || pool.noBalance || 0) === 0)
          ? `<p class="hint">Noch nichts on-chain. Mining-Auszahlungen kommen, sobald 666pool <strong>2 VDS</strong> unbezahltes Guthaben erreicht.</p>`
          : ""
      }
      ${
        history.length
          ? `<ul class="txlist">${history
              .map(
                (id) =>
                  `<li><a href="${esc(explorerTxUrl(id))}" target="_blank" rel="noreferrer">${esc(id.slice(0, 10))}…${esc(
                    id.slice(-8),
                  )}</a></li>`,
              )
              .join("")}</ul>`
          : ""
      }
      <div class="row">
        <button type="button" class="ghost" id="btn-refresh">Aktualisieren</button>
        <a class="ghost btn-link" href="${esc("https://vdscool.com/address/" + wallet!.address)}" target="_blank" rel="noreferrer">Im Explorer öffnen</a>
      </div>
    </section>
  `;
}

function sendTab() {
  const spendable = onChain ? onChain.balance : 0n;
  return `
    <section class="card">
      <h2>VDS senden</h2>
      <p class="hint">Nur an eine Adresse, die mit <strong>Vc</strong> beginnt. Gebühr fest ${formatVds(DEFAULT_FEE_SATS)} VDS (üblich on-chain).</p>
      ${
        spendable === 0n && !loading
          ? `<div class="banner err">Kein ausgebbares Guthaben. Pool zahlt erst ab 2 VDS auf diese Adresse.</div>`
          : `<p class="balance small">${esc(formatVds(spendable))} <span>VDS verfügbar</span></p>`
      }
      ${lastTxid ? `<div class="banner ok">Gesendet. <a href="${esc(explorerTxUrl(lastTxid))}" target="_blank" rel="noreferrer">Transaktion ansehen</a></div>` : ""}
      <label class="block">Empfänger (Vc…)
        <input id="send-to" type="text" autocomplete="off" spellcheck="false" value="${esc(sendTo)}" placeholder="Vc…" />
      </label>
      <label class="block">Betrag in VDS
        <input id="send-amt" type="text" inputmode="decimal" value="${esc(sendAmt)}" placeholder="z. B. 1.5" />
      </label>
      <div class="row">
        <button type="button" class="primary" id="btn-send" ${sendBusy || spendable === 0n ? "disabled" : ""}>${
          sendBusy ? "Sende…" : "Senden"
        }</button>
        <button type="button" class="ghost" id="btn-max" ${spendable <= DEFAULT_FEE_SATS ? "disabled" : ""}>Alles minus Gebühr</button>
      </div>
    </section>
  `;
}

function poolTab() {
  if (!pool) {
    return `<section class="card"><h2>666pool</h2><p class="hint">${
      loading ? "Lade Pool…" : "Keine Pool-Daten — Adresse ist dem Pool noch unbekannt oder der Miner hat noch kein Share."
    }</p></section>`;
  }
  const pending = Number(pool.noBalance ?? pool.balance ?? 0);
  const paid = Number(pool.allPay ?? 0);
  const workers = pool.worksOnline ?? [];
  return `
    <section class="card">
      <h2>666pool Auszahlung</h2>
      <p class="hint">Unbezahlt wird automatisch auf deine Vc-Adresse geschickt, sobald <strong>2 VDS</strong> erreicht sind (Pool-Minimum).</p>
      <div class="stats">
        <div><span>Unbezahlt</span><strong>${esc(String(pending))}</strong></div>
        <div><span>Ausgezahlt</span><strong>${esc(String(paid))}</strong></div>
        <div><span>24h</span><strong>${esc(String(pool.dayIncome ?? 0))}</strong></div>
        <div><span>Hashrate</span><strong>${esc(String(pool.nowHash ?? 0))} ${esc(String(pool.dayHash || "").trim())}</strong></div>
      </div>
      <p class="hint">Worker online ${pool.onWorks ?? 0} · offline ${pool.offWorks ?? 0}</p>
      ${
        workers.length
          ? `<ul class="txlist">${workers
              .map((w) => `<li>${esc(String(w.name ?? "worker"))} · ${esc(String(w.hash ?? "—"))}</li>`)
              .join("")}</ul>`
          : ""
      }
      <p class="hint"><a href="https://www.666pool.com/#/miners-income/VDS/${esc(
        wallet!.address,
      )}" target="_blank" rel="noreferrer">Auf 666pool öffnen</a></p>
    </section>
  `;
}

function keysTab() {
  return `
    <section class="card warn">
      <h2>Privater Schlüssel</h2>
      <p>Ohne diese Zeile sind die Coins weg. Nicht an den Pool, nicht in HiveOS, nicht in die Cloud.</p>
      <pre class="secret">${reveal ? esc(wallet!.wif) : "••••••••••••••••••••••••••••••••••••••••"}</pre>
      <div class="row">
        <button type="button" class="ghost" id="btn-reveal">${reveal ? "Verbergen" : "Schlüssel anzeigen"}</button>
        <button type="button" class="ghost" data-copy="wif">WIF kopieren</button>
        <button type="button" class="ghost" id="btn-download">Backup herunterladen</button>
      </div>
    </section>
    <div class="row">
      <button type="button" class="ghost danger" id="btn-reset">Wallet aus diesem Browser löschen</button>
    </div>
  `;
}

function walletScreen() {
  return `
    ${header()}
    ${tabs()}
    ${tab === "receive" ? receiveTab() : ""}
    ${tab === "send" ? sendTab() : ""}
    ${tab === "pool" ? poolTab() : ""}
    ${tab === "keys" ? keysTab() : ""}
  `;
}

function bind() {
  document.getElementById("btn-gen")?.addEventListener("click", () => {
    persist = (document.getElementById("persist") as HTMLInputElement | null)?.checked ?? persist;
    openWallet(generateKeypair());
  });
  document.getElementById("btn-import-toggle")?.addEventListener("click", () => {
    importOpen = !importOpen;
    render();
  });
  document.getElementById("btn-import")?.addEventListener("click", () => {
    persist = (document.getElementById("persist") as HTMLInputElement | null)?.checked ?? persist;
    const raw = (document.getElementById("wif-in") as HTMLTextAreaElement | null)?.value ?? "";
    try {
      openWallet(importWif(raw));
    } catch (e) {
      errorText = "Import fehlgeschlagen: " + String(e);
      render();
    }
  });
  document.getElementById("persist")?.addEventListener("change", (ev) => {
    persist = (ev.target as HTMLInputElement).checked;
  });
  app.querySelectorAll<HTMLButtonElement>("[data-tab]").forEach((b) => {
    b.addEventListener("click", () => {
      tab = b.dataset.tab as Tab;
      render();
      if (tab === "receive") void paintQr();
    });
  });
  document.getElementById("btn-refresh")?.addEventListener("click", () => void refresh());
  document.getElementById("btn-reveal")?.addEventListener("click", () => {
    reveal = !reveal;
    render();
  });
  document.getElementById("btn-reset")?.addEventListener("click", () => {
    clearWallet();
    wallet = null;
    screen = "empty";
    onChain = null;
    pool = null;
    lastTxid = "";
    errorText = "";
    render();
  });
  document.getElementById("btn-download")?.addEventListener("click", onDownload);
  document.getElementById("btn-max")?.addEventListener("click", () => {
    if (!onChain) return;
    const max = onChain.balance - DEFAULT_FEE_SATS;
    sendAmt = max > 0n ? formatVds(max) : "0";
    render();
  });
  document.getElementById("btn-send")?.addEventListener("click", () => void onSend());
  const toEl = document.getElementById("send-to") as HTMLInputElement | null;
  const amtEl = document.getElementById("send-amt") as HTMLInputElement | null;
  toEl?.addEventListener("input", () => {
    sendTo = toEl.value;
  });
  amtEl?.addEventListener("input", () => {
    sendAmt = amtEl.value;
  });
  app.querySelectorAll<HTMLButtonElement>("[data-copy]").forEach((b) => {
    b.addEventListener("click", () => {
      const kind = b.dataset.copy;
      const text = kind === "wif" ? wallet?.wif : wallet?.address;
      if (!text) return;
      void navigator.clipboard.writeText(text).then(
        () => {
          b.textContent = "Kopiert";
          setTimeout(() => render(), 900);
        },
        () => {
          errorText = "Zwischenablage blockiert — Text manuell markieren.";
          render();
        },
      );
    });
  });
}

function openWallet(kp: VdsKeypair) {
  wallet = kp;
  screen = "wallet";
  tab = "receive";
  reveal = false;
  importOpen = false;
  errorText = "";
  noticeText = "";
  saveWallet(kp, persist);
  render();
  void refresh();
}

async function onSend() {
  if (!wallet || sendBusy) return;
  errorText = "";
  noticeText = "";
  sendTo = (document.getElementById("send-to") as HTMLInputElement | null)?.value.trim() ?? sendTo;
  sendAmt = (document.getElementById("send-amt") as HTMLInputElement | null)?.value.trim() ?? sendAmt;
  if (!isVcAddress(sendTo)) {
    errorText = "Empfänger muss eine gültige Vc-Adresse sein.";
    render();
    return;
  }
  let amount: bigint;
  try {
    amount = parseVds(sendAmt);
  } catch (e) {
    errorText = String(e);
    render();
    return;
  }
  sendBusy = true;
  render();
  try {
    const utxos = (await fetchUtxos(wallet.address))
      .filter((u) => !u.isStake && (u.confirmations ?? 0) > 0)
      .map((u) => ({
        transactionId: u.transactionId,
        outputIndex: u.outputIndex,
        scriptPubKey: u.scriptPubKey,
        value: sats(u.value),
      }));
    const built = buildSignedSend({
      wif: wallet.wif,
      fromAddress: wallet.address,
      toAddress: sendTo,
      amount,
      utxos,
    });
    const res = await broadcastRaw(built.hex);
    lastTxid = res.txid || res.hash || res.id || built.txid;
    noticeText = `Gesendet (${built.size} Byte, Gebühr ${formatVds(built.fee)} VDS).`;
    sendAmt = "";
    await refresh();
  } catch (e) {
    errorText = "Senden fehlgeschlagen: " + String(e);
  } finally {
    sendBusy = false;
    render();
  }
}

function onDownload() {
  if (!wallet) return;
  const body =
    "VDS wallet backup\n" +
    "Address: " +
    wallet.address +
    "\nWIF: " +
    wallet.wif +
    "\nKeep this file offline. HiveOS wallet field: the Address line only.\n";
  const blob = new Blob([body], { type: "text/plain" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = "vds-wallet-backup.txt";
  a.click();
  URL.revokeObjectURL(a.href);
}

function render() {
  app.innerHTML = `<main class="wrap">${screen === "empty" ? emptyScreen() : walletScreen()}</main>`;
  bind();
  if (screen === "wallet" && tab === "receive") void paintQr();
}

const restored = loadStored();
if (restored) {
  persist = !!localStorage.getItem(STORE_LOCAL);
  openWallet(restored);
} else {
  render();
}
