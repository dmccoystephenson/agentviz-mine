// Regenerates every README image in docs/media/ from the FIXTURE feed, with
// scene time stepped by ?drive=1 so frames repeat. Takes a few minutes under
// SwiftShader.  NODE_PATH=/root/arcade-watch/node_modules node tests/screenshots.cjs [only-name]
const path = require("path");
const fs = require("fs");
const os = require("os");
const {execFileSync} = require("child_process");
const {ROOT, startServer, open, launch} = require("./harness.cjs");
const MEDIA = path.join(ROOT, "docs/media");
const only = process.argv[2] || null;
const HALF_PI = Math.PI / 2;

// Each shot: a viewport, then a setup run in the page (cam + selection).
const SHOTS = [
  {name: "hero", setup: `__mine.select(null); __mine.fit(true);`},
  {name: "angle-high", setup: `__mine.fit(true); __mine.look({yaw: ${HALF_PI + 0.55}, pitch: 0.5, dist: __mine.cam.dist * 0.75});`},
  {name: "angle-low", setup: `__mine.fit(true); __mine.look({yaw: ${HALF_PI - 0.7}, pitch: 0.05, dist: __mine.cam.dist * 0.6, ty: -6});`},
  {name: "lead-subagents", setup: `const e = __mine.findSession("Otto"); __mine.look({tx: e.cur.x, ty: e.cur.face / 2 + 0.5, dist: 26, yaw: ${HALF_PI}, pitch: 0.12});`},
  {name: "waiting", setup: `const e = __mine.findSession("Thistle"); __mine.look({tx: e.cur.x, ty: 1.5, dist: 19, yaw: ${HALF_PI + 0.2}, pitch: 0.14});`},
  {name: "idle-offline", setup: `const a = __mine.findSession("Lantern"), b = __mine.findSession("Tally"); __mine.look({tx: (a.cur.x + b.cur.x) / 2, ty: -2.4, dist: 15, yaw: ${HALF_PI}, pitch: 0.12});`},
  {name: "stalled-solo", setup: `const a = __mine.findSession("Anvil"), b = __mine.findSession("Scout"); __mine.look({tx: (a.cur.x + b.cur.x) / 2, ty: a.cur.face + 1.4, dist: 11, yaw: ${HALF_PI}, pitch: 0.1});`},
  {name: "resting-subagent", setup: `const f = __mine.findFollower("tests + docs"); const p = __mine.posOf(f); __mine.select(f); __mine.look({tx: p.x, ty: p.y + 1, dist: 9, yaw: ${HALF_PI}, pitch: 0.1});`},
  {name: "panel-desktop", setup: `const e = __mine.findSession("Wren"); __mine.select(e); const p = __mine.posOf(e); __mine.look({tx: e.cur.x - 3, ty: e.cur.face / 2, dist: 24, yaw: ${HALF_PI}, pitch: 0.12});`},
  {name: "phone", viewport: {width: 390, height: 844}, dpr: 3, mobile: true,
    setup: `const e = __mine.findSession("Thistle"); __mine.select(e); __mine.look({tx: e.cur.x, ty: -4, dist: 24, yaw: ${HALF_PI}, pitch: 0.14});`},
];

async function shoot(browser, s) {
  const {ctx, page, errors} = await open(browser, {query: "&drive=1", viewport: s.viewport || {width: 1440, height: 810},
    dpr: s.dpr || 1, mobile: s.mobile});
  await page.evaluate(() => window.__strataKit.drive(0.5));
  await page.evaluate(s.setup);
  for (let i = 0; i < 3; i++) await page.evaluate((t) => window.__strataKit.drive(t), 2.3 + i * 0.4);
  await page.screenshot({path: path.join(MEDIA, s.name + ".png"), timeout: 180000});
  await ctx.close();
  if (errors.length) throw new Error(`${s.name}: console errors: ${errors.join(" | ")}`);
  console.log("saved", s.name + ".png");
}

async function gif(browser) {
  const {ctx, page, errors} = await open(browser, {query: "&drive=1", viewport: {width: 960, height: 540}});
  await page.evaluate(() => window.__strataKit.drive(0.5));
  await page.evaluate(`const e = __mine.findSession("Wren"); __mine.look({tx: e.cur.x, ty: e.cur.face / 2 + 0.5, dist: 21, yaw: ${HALF_PI}, pitch: 0.12});`);
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "mine-gif-"));
  const fps = 8, seconds = 5;
  for (let i = 0; i < fps * seconds; i++) {
    const t = 3 + i / fps;
    // A slow drift across the lead's workings while everything animates.
    await page.evaluate(([t, i]) => { __mine.look({yaw: Math.PI / 2 - 0.25 + i * 0.012}); window.__strataKit.drive(t); }, [t, i]);
    await page.screenshot({path: path.join(dir, `f${String(i).padStart(3, "0")}.png`), timeout: 180000});
  }
  await ctx.close();
  if (errors.length) throw new Error("gif: console errors: " + errors.join(" | "));
  const out = path.join(MEDIA, "mine.gif");
  // Pillow (no ffmpeg needed): 800 px wide, one shared adaptive palette.
  execFileSync("python3", ["-c", `
import glob, sys
from PIL import Image
files = sorted(glob.glob(sys.argv[1] + "/f*.png"))
frames = [Image.open(f).convert("RGB") for f in files]
w = 800; h = round(frames[0].height * w / frames[0].width)
frames = [f.resize((w, h), Image.LANCZOS) for f in frames]
pal = frames[len(frames) // 2].quantize(colors=160, method=Image.MEDIANCUT)
out = [f.quantize(palette=pal, dither=Image.FLOYDSTEINBERG) for f in frames]
out[0].save(sys.argv[2], save_all=True, append_images=out[1:], duration=125, loop=0, optimize=True)
`, dir, out]);
  fs.rmSync(dir, {recursive: true, force: true});
  console.log("saved mine.gif", fs.statSync(out).size, "B");
}

(async () => {
  fs.mkdirSync(MEDIA, {recursive: true});
  const server = await startServer();
  const browser = await launch();
  let code = 0;
  try {
    for (const s of SHOTS) if (!only || only === s.name) await shoot(browser, s);
    if (!only || only === "gif") await gif(browser);
  } catch (e) {
    console.error(e.stack || String(e));
    code = 1;
  } finally {
    await browser.close();
    server.kill();
  }
  process.exit(code);
})();
