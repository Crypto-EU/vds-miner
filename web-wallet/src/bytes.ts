/** Little helpers for VDS binary encoding (Bitcoin-style). */

export function concat(...parts: Uint8Array[]): Uint8Array {
  const n = parts.reduce((a, p) => a + p.length, 0);
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) {
    out.set(p, o);
    o += p.length;
  }
  return out;
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

/** Explorer hashes are display-order (reversed vs internal uint256). */
export function displayHexToInternal(hex: string): Uint8Array {
  const b = hexToBytes(hex);
  return b.reverse();
}

export function compactSize(n: number): Uint8Array {
  if (n < 0) throw new Error("negative compact size");
  if (n < 253) return new Uint8Array([n]);
  if (n < 0x10000) {
    const o = new Uint8Array(3);
    o[0] = 0xfd;
    o[1] = n & 0xff;
    o[2] = (n >> 8) & 0xff;
    return o;
  }
  if (n < 0x100000000) {
    const o = new Uint8Array(5);
    o[0] = 0xfe;
    const v = n >>> 0;
    o[1] = v & 0xff;
    o[2] = (v >> 8) & 0xff;
    o[3] = (v >> 16) & 0xff;
    o[4] = (v >> 24) & 0xff;
    return o;
  }
  throw new Error("compact size zu groß");
}

export function withCompact(data: Uint8Array): Uint8Array {
  return concat(compactSize(data.length), data);
}

export function u32le(n: number): Uint8Array {
  const o = new Uint8Array(4);
  const v = n >>> 0;
  o[0] = v & 0xff;
  o[1] = (v >> 8) & 0xff;
  o[2] = (v >> 16) & 0xff;
  o[3] = (v >> 24) & 0xff;
  return o;
}

export function i32le(n: number): Uint8Array {
  return u32le(n);
}

export function i64le(n: bigint): Uint8Array {
  const o = new Uint8Array(8);
  let v = BigInt.asUintN(64, n);
  for (let i = 0; i < 8; i++) {
    o[i] = Number(v & 0xffn);
    v >>= 8n;
  }
  return o;
}
