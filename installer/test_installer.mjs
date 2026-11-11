// Self-check for the installer core (the <script id="core"> in installer/template.html).
//   node installer/test_installer.mjs                       synthetic SampQOL-shaped folder (CI)
//   node installer/test_installer.mjs <autorun dir> <out>   a real SampQOL download; writes the patched folder
import fs from "node:fs"; import path from "node:path"; import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process"; import assert from "node:assert/strict";

const sha = b => createHash("sha256").update(b).digest("hex");
const SALT = "f7bed1c9-7e90-4731-9194-c996666b4331";
const [src, outDir] = process.argv.slice(2);

let files;
if (src) {
  files = new Map(fs.readdirSync(src).filter(f => !f.startsWith(".")).map(f => [f, new Uint8Array(fs.readFileSync(path.join(src, f)))]));
} else {
  const run = 'DIR="$1"\nSO=/tmp/mpcqol.so\nsplash() {\n    if [ -f "$DIR/qol-splash.raw" ]; then\n' +
              '        cat "$DIR/qol-splash.raw" > /dev/fb0\n    fi 2>/dev/null\n}\n( export LD_PRELOAD="$SO"\n  true )\n';
  const so = Buffer.from("fake mpcqol");
  files = new Map([["mpcqol.so", so], ["qol-run.sh", Buffer.from(run)], ["autorun.json", Buffer.from("{}")],
                   ["autorun.check", Buffer.from("x")]].map(([k, v]) => [k, new Uint8Array(v)]));
  files.set("autorun.sh", new Uint8Array(Buffer.from(`PINS="${sha(files.get("qol-run.sh"))}  qol-run.sh\n${sha(so)}  mpcqol.so"\n`)));
}

// Synthetic mode: the template with a placeholder add-on (no build needed, runs in CI).
// Real mode: the BUILT page (run ./build.sh first), so the output carries the real add-ons and can boot.
const page = src ? "../docs/install/index.html" : "./template.html";
let core = fs.readFileSync(new URL(page, import.meta.url), "utf8").match(/<script id="core">([\s\S]*?)<\/script>/)[1];
core = core.replace("__SAMPQOL_HASHES__", "{}")
           .replace("__ADDON_FILES__", JSON.stringify({ "hs/hs-addons.sh": Buffer.from("HS_PRELOAD=\n").toString("base64") }));
// Also accept the given SampQOL build, so older downloads can exercise the patch path (official 0.2.0 is already listed).
core += `\nSAMPQOL[${JSON.stringify(sha(files.get("mpcqol.so")))}] ??= "test";`;
const patch = new Function(core + "; return patch;")();

const { out } = await patch(files);
const text = f => Buffer.from(out.get(f)).toString();
// firmware signature + SampQOL's own pins must both pass
assert.equal(text("autorun.check"), sha(Buffer.concat([out.get("autorun.json"), out.get("autorun.sh"), Buffer.from(SALT)])));
for (const [, hash, name] of text("autorun.sh").matchAll(/([0-9a-f]{64})  ([^\n"]+)/g))
  if (out.has(name)) assert.equal(sha(out.get(name)), hash, `${name} pin`);
assert.match(text("qol-run.sh"), /\. "\$DIR\/hs\/hs-addons.sh"/);
assert.match(text("qol-run.sh"), /LD_PRELOAD="\$HS_PRELOAD\$SO"/);
assert.match(text("qol-run.sh"), /cat "\$DIR\/qol-splash.raw" > \/dev\/fb0\n.*hs-splash.raw/, "SampQOL splash stays first");
assert.equal(spawnSync("sh", ["-n"], { input: text("qol-run.sh") }).status, 0, "qol-run.sh syntax");
await assert.rejects(patch(out), /already installed/);
const bad = new Map(files); bad.set("qol-run.sh", new Uint8Array([1]));
await assert.rejects(patch(bad), /changed/);
const wrong = new Map(files); wrong.set("mpcqol.so", new Uint8Array([2]));
await assert.rejects(patch(wrong), /not SampQOL/);
if (src) for (const f of ["hs-addons.sh", "hs-splash.raw", "hs-flavor.so", "hs-eq.so", "hs-fx.so", "hs-seq.so", "hs-pro-fx-ladspa.so"])
  assert.ok(out.get("hs/" + f)?.length > 100, `real mode must carry the built hs/${f}`);

if (outDir) for (const [p, d] of out) { fs.mkdirSync(path.dirname(path.join(outDir, p)), { recursive: true }); fs.writeFileSync(path.join(outDir, p), d); }
console.log(`installer ok (${src ? "real" : "synthetic"} SampQOL, ${out.size} files)`);
