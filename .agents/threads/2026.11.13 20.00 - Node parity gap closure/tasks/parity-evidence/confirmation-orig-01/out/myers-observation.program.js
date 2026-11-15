import { rngCreate } from "./src/core/model/testDocs.ts";
const r7 = rngCreate(7); const seed7Raw3 = [r7(), r7(), r7()].map((x) => x * 4294967296);
if (!seed7Raw3.every((x) => Number.isInteger(x))) throw new Error("non-integer raw");
const fnv = (buf) => { let h = 0x811c9dc5; for (const byte of buf) h = Math.imul(h ^ byte, 0x01000193) >>> 0; return h; };
const out = (seed, trials, s, first3) => { const buf = Buffer.from(s, "ascii"); const h = fnv(buf); return { seed, trials, byteLength: buf.length, fnv1a32Hex: h.toString(16).padStart(8, "0"), fnv1a32Dec: h, first3 }; };
const seeds = [];
{ const rng = rngCreate(7); let s = ""; const first3 = []; let n = 0;
for (let t = 0; t < 500; t++) {
const a = Array.from({ length: Math.floor(rng() * 14) }, () => "abc"[Math.floor(rng() * 3)]);
const b = Array.from({ length: Math.floor(rng() * 14) }, () => "abc"[Math.floor(rng() * 3)]);
const line = a.join("") + "|" + b.join(""); if (t < 3) first3.push(line); s += line + "\n"; n++; }
seeds.push(out(7, n, s, first3)); }
{ const rng = rngCreate(11); let s = ""; const first3 = []; let n = 0;
for (let t = 0; t < 300; t++) {
const a = Array.from({ length: Math.floor(rng() * 10) }, () => "xyz"[Math.floor(rng() * 3)]);
const b = Array.from({ length: Math.floor(rng() * 10) }, () => "xyz"[Math.floor(rng() * 3)]);
const line = a.join("") + "|" + b.join(""); if (t < 3) first3.push(line); s += line + "\n"; n++; }
seeds.push(out(11, n, s, first3)); }
console.log(JSON.stringify({ seed7Raw3, seeds }));