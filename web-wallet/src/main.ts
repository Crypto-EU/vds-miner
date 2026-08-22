import "./style.css";
import {
  generateKeypair,
  importWif,
  selfTest,
  type VdsKeypair,
} from "./vds";

const app = document.querySelector<HTMLDivElement>("#app")!;

type View = "empty" | "wallet" | "error";

let view: View = "empty";
let wallet: VdsKeypair | null = null;
let errorText = "";
let reveal = false;
let importOpen = false;

function esc(s: string): string {
  return s
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function render() {
  const tests = selfTest();
  const testOk = tests.length === 0;

  app.innerHTML = `
    <main class="wrap">
      <header>
        <p class="eyebrow">VDS · Vollar · 666pool</p>
        <h1>Browser-Wallet</h1>
        <p class="lead">Erzeugt eine echte VDS-Adresse <strong>Vc…</strong> direkt im Browser. Es gibt keine offizielle MetaMask-/Chrome-Wallet für diese Chain — 0x-Adressen funktionieren beim Pool nicht.</p>
      </header>

      ${
        !testOk
          ? `<div class="banner err" role="alert">Selbsttest fehlgeschlagen: ${esc(tests.join("; "))}</div>`
          : `<div class="banner ok">Prüfsumme und Prefix <code>Vc</code> sind gültig. Schlüssel verlassen diesen Rechner nicht.</div>`
      }

      ${view === "error" ? `<div class="banner err" role="alert">${esc(errorText)}</div>` : ""}

      ${
        view !== "wallet"
          ? `
        <section class="card empty">
          <h2>Noch keine Adresse</h2>
          <p>Ein Klick erzeugt ein neues Schlüsselpaar. Danach die <strong>Vc-Adresse</strong> ins HiveOS-Flight-Sheet kopieren und den <strong>privaten Schlüssel</strong> sichern.</p>
          <div class="row">
            <button type="button" class="primary" id="btn-gen">Neue Vc-Adresse erzeugen</button>
            <button type="button" class="ghost" id="btn-import-toggle">Vorhandenen Schlüssel einfügen</button>
          </div>
          ${
            importOpen
              ? `
            <label class="block">
              WIF oder Backup
              <textarea id="wif-in" rows="3" placeholder="beginnt typischerweise mit 5, K oder L"></textarea>
            </label>
            <button type="button" class="primary" id="btn-import">Adresse wiederherstellen</button>
          `
              : ""
          }
        </section>
      `
          : `
        <section class="card">
          <h2>Deine Mining-Adresse</h2>
          <p class="hint">Ins HiveOS-Feld <em>Wallet</em> — ohne Worker-Namen, ohne 0x.</p>
          <div class="addr" id="addr">${esc(wallet!.address)}</div>
          <div class="row">
            <button type="button" class="primary" data-copy="address">Adresse kopieren</button>
            <button type="button" class="ghost" id="btn-download">Backup herunterladen</button>
          </div>
        </section>
        <section class="card warn">
          <h2>Privater Schlüssel</h2>
          <p>Ohne diese Zeile gehören die Coins niemandem mehr — auch nicht dir. Nicht an den Pool schicken, nicht fotografieren, nicht in der Cloud speichern.</p>
          <pre class="secret">${reveal ? esc(wallet!.wif) : "••••••••••••••••••••••••••••••••••••••••"}</pre>
          <div class="row">
            <button type="button" class="ghost" id="btn-reveal">${reveal ? "Verbergen" : "Schlüssel anzeigen"}</button>
            <button type="button" class="ghost" data-copy="wif">WIF kopieren</button>
          </div>
        </section>
        <div class="row">
          <button type="button" class="ghost danger" id="btn-reset">Diese Sitzung löschen</button>
        </div>
      `
      }

      <section class="foot">
        <h2>Was das nicht ist</h2>
        <ul>
          <li>Kein MetaMask und kein BNB-Token namens „Vollar“ — das wäre eine andere Chain.</li>
          <li>Kein Online-Tresor: es gibt keinen Server, der deine Coins hält.</li>
          <li>Zum späteren <em>Ausgeben</em> den WIF in eine volle VDS-Node-Wallet importieren.</li>
        </ul>
      </section>
    </main>
  `;

  document.getElementById("btn-gen")?.addEventListener("click", onGenerate);
  document.getElementById("btn-import-toggle")?.addEventListener("click", () => {
    importOpen = !importOpen;
    render();
  });
  document.getElementById("btn-import")?.addEventListener("click", onImport);
  document.getElementById("btn-reveal")?.addEventListener("click", () => {
    reveal = !reveal;
    render();
  });
  document.getElementById("btn-reset")?.addEventListener("click", () => {
    wallet = null;
    view = "empty";
    reveal = false;
    render();
  });
  document.getElementById("btn-download")?.addEventListener("click", onDownload);
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
          errorText = "Zwischenablage blockiert — Adresse manuell markieren.";
          view = "error";
          render();
        },
      );
    });
  });
}

function onGenerate() {
  try {
    wallet = generateKeypair();
    view = "wallet";
    reveal = false;
    errorText = "";
  } catch (e) {
    view = "error";
    errorText = "Konnte keinen Schlüssel erzeugen: " + String(e);
  }
  render();
}

function onImport() {
  const raw = (document.getElementById("wif-in") as HTMLTextAreaElement | null)?.value ?? "";
  try {
    wallet = importWif(raw);
    view = "wallet";
    reveal = false;
    errorText = "";
    importOpen = false;
  } catch (e) {
    view = "error";
    errorText = "Import fehlgeschlagen: " + String(e);
  }
  render();
}

function onDownload() {
  if (!wallet) return;
  const body =
    "VDS mining wallet backup\n" +
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

render();
