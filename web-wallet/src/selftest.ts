import { generateKeypair, importWif, selfTest, decodeBase58Check, SAMPLE_VC, isVcAddress, p2pkhScriptFromAddress } from "./vds.ts";
import { dummyTxSize } from "./tx.ts";
import { parseVds, formatVds, COIN } from "./amount.ts";

const errs = selfTest();
if (errs.length) {
  console.error(errs);
  process.exit(1);
}
const p = decodeBase58Check(SAMPLE_VC);
if (p[0] !== 0x10 || p[1] !== 0x1c) process.exit(1);
const kp = generateKeypair();
if (!kp.address.startsWith("Vc")) process.exit(1);
if (importWif(kp.wif).address !== kp.address) process.exit(1);
if (!isVcAddress(kp.address)) process.exit(1);
if (p2pkhScriptFromAddress(kp.address).length !== 25) process.exit(1);
if (parseVds("1.5") !== COIN + COIN / 2n) {
  console.error("parseVds");
  process.exit(1);
}
if (formatVds(COIN) !== "1") {
  console.error("formatVds", formatVds(COIN));
  process.exit(1);
}
const sz = dummyTxSize();
if (sz < 300 || sz > 500) {
  console.error("dummy tx size", sz);
  process.exit(1);
}
console.log("ok", kp.address, "txbytes", sz);
