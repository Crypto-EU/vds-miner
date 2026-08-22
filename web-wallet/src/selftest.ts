import { generateKeypair, importWif, selfTest, decodeBase58Check, SAMPLE_VC } from "./vds.ts";

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
console.log("ok", kp.address);
