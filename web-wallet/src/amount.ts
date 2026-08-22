/** 1 VDS = 10^8 satoshis (same as Bitcoin COIN). */
export const COIN = 100_000_000n;
export const DEFAULT_FEE_SATS = 10_000n; // 0.0001 VDS — typical on-chain fee

export function parseVds(text: string): bigint {
  const t = text.trim().replace(",", ".");
  if (!t) throw new Error("Betrag fehlt");
  if (!/^\d+(\.\d{1,8})?$/.test(t)) throw new Error("Betrag ungültig (max. 8 Nachkommastellen)");
  const [a, b = ""] = t.split(".");
  const frac = (b + "00000000").slice(0, 8);
  return BigInt(a) * COIN + BigInt(frac);
}

export function formatVds(sats: bigint, digits = 8): string {
  const neg = sats < 0n;
  const v = neg ? -sats : sats;
  const whole = v / COIN;
  const frac = (v % COIN).toString().padStart(8, "0").replace(/0+$/, "");
  const body = frac ? `${whole.toString()}.${frac}` : whole.toString();
  const shown =
    digits < 8 && frac
      ? `${whole.toString()}.${(v % COIN).toString().padStart(8, "0").slice(0, digits).replace(/0+$/, "") || "0"}`
      : body;
  return (neg ? "-" : "") + shown;
}

