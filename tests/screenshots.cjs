// Regenerates every README image in docs/media/ from the DEMO feed (never live):
//   npm run screenshots [-- name ...]
// Stills are drawn with ?drive=1 (the kit draws one frame on demand at a
// chosen scene time), so they repeat. The GIF steps scene time frame by frame
// and is encoded with Pillow (tests/make_gif.py: a palette per frame, which
// keeps the shirt colours that the kit's shared palette loses).
const {execFileSync} = require("child_process");
const fs = require("fs");
const os = require("os");
const path = require("path");
const h = require("../vendor/agentviz-kit/tests/harness.cjs");
const screens = require("../vendor/agentviz-kit/tests/screens.cjs");

const REPO = path.resolve(__dirname, "..");
const MEDIA = path.join(REPO, "docs/media");
const PORT = Number(process.env.MINE_TEST_PORT || 9112);
const READY = "window.__page && window.__page.polls > 0 && window.__mine.sessions.size > 0";
const only = new Set(process.argv.slice(2));
const want = (name) => only.size === 0 || only.has(name);
const DESKTOP = {viewport: {width: 1440, height: 810}};
const PHONE = {viewport: {width: 390, height: 844}, dpr: 3, mobile: true};

// Camera helpers run in the page: frame a session's miner, a subagent, or two sessions.
const HELPERS = `
window.__shots = {
  t: 2.3,
  cam(c) { window.__page.look(Object.assign({yaw: Math.PI / 2, pitch: 0.12}, c)); },
  session(name) { return window.__mine.findSession(name); },
  miner(name, c) { const e = this.session(name), p = window.__mine.minerPos(e); this.cam(Object.assign({tx: p.x, ty: p.y + 1.2}, c)); },
  shaft(name, c) { const e = this.session(name); this.cam(Object.assign({tx: e.cur.x, ty: e.cur.face / 2 + 0.5}, c)); },
  between(a, b, c) { const x = (this.session(a).cur.x + this.session(b).cur.x) / 2; this.cam(Object.assign({tx: x}, c)); },
  draw() { window.__shots.t += 0.4; window.__page.kit.drive(window.__shots.t); },
};`;

async function openPage(browser, server, options) {
  const opened = await h.open(browser, `${server.base}/?fixture=1&drive=1`, Object.assign({ready: READY}, options));
  await h.stillAnimations(opened.page);
  await opened.page.evaluate(() => window.__page.stopPolling());
  await opened.page.evaluate(HELPERS);
  await opened.page.evaluate(() => window.__page.kit.drive(0.5));
  return opened;
}

async function shoot(page, name, setup) {
  if (!want(name)) return;
  await page.evaluate(() => { window.__page.select(null); window.__page.holdPulse(null); window.__shots.t = 2.3; });
  await page.evaluate(setup);
  await screens.still(page, path.join(MEDIA, name + ".png"), () => window.__shots.draw());
}

const HALF_PI = "Math.PI / 2";
const DESKTOP_SHOTS = [
  ["hero", "window.__page.fit(true)"],
  ["angle-high", `window.__page.fit(true); window.__shots.cam({yaw: ${HALF_PI} + 0.55, pitch: 0.5, dist: window.__page.cam.dist * 0.75})`],
  ["angle-low", `window.__page.fit(true); window.__shots.cam({yaw: ${HALF_PI} - 0.7, pitch: 0.05, dist: window.__page.cam.dist * 0.6, ty: -6})`],
  ["lead-subagents", "window.__shots.shaft('Otto', {dist: 30})"],
  ["waiting", `window.__shots.shaft('Thistle', {ty: 1.5, dist: 19, yaw: ${HALF_PI} + 0.2, pitch: 0.14})`],
  ["question", "window.__shots.cam({tx: window.__shots.session('Juniper').cur.x, ty: 1.6, dist: 9, pitch: 0.1})"],
  ["idle-offline", "window.__shots.between('Lantern', 'Tally', {ty: -2.4, dist: 17})"],
  ["stalled", "window.__shots.miner('Patch', {dist: 10})"],
  ["stalled-subagent", "const f = window.__mine.findFollower('cache warm-up'), p = window.__mine.posOf(f); window.__page.select(f.id); window.__shots.cam({tx: p.x, ty: p.y + 1, dist: 9})"],
  ["resting-subagent", "const f = window.__mine.findFollower('tests + docs'), p = window.__mine.posOf(f); window.__page.select(f.id); window.__shots.cam({tx: p.x, ty: p.y + 1, dist: 9})"],
  ["tool-call", "const e = window.__shots.session('Anvil'); window.__page.holdPulse((id) => (id === e.id ? 0.7 : 0)); window.__shots.miner('Anvil', {dist: 8})"],
  ["panel-desktop", "const e = window.__shots.session('Otto'); window.__page.select(e.id); window.__shots.cam({tx: e.cur.x - 4, ty: e.cur.face / 2, dist: 30})"],
];

async function desktopStills(browser, server) {
  const {ctx, page, errors} = await openPage(browser, server, DESKTOP);
  for (const [name, setup] of DESKTOP_SHOTS) await shoot(page, name, setup);
  h.assertNoErrors(errors);
  await ctx.close();
}

async function phone(browser, server) {
  if (!want("phone")) return;
  const {ctx, page, errors} = await openPage(browser, server, PHONE);
  await shoot(page, "phone", "const e = window.__shots.session('Thistle'); window.__page.select(e.id); window.__shots.cam({tx: e.cur.x, ty: -4, dist: 24, pitch: 0.14})");
  h.assertNoErrors(errors);
  await ctx.close();
}

// The GIF: five seconds of Wren's workings at 8 frames a second, the camera drifting across.
async function workingsGif(browser, server) {
  if (!want("mine")) return;
  const {ctx, page, errors} = await openPage(browser, server, {viewport: {width: 960, height: 540}});
  await page.evaluate(() => window.__shots.shaft("Wren", {dist: 21}));
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "mine-gif-"));
  const FPS = 8;
  await screens.frames(page, tmp, FPS * 5, (i) => {
    window.__page.look({yaw: Math.PI / 2 - 0.25 + i * 0.012});
    window.__page.kit.drive(3 + i / 8);
  });
  execFileSync("python3", [path.join(__dirname, "make_gif.py"), tmp, path.join(MEDIA, "mine.gif"), String(FPS), "800"],
    {stdio: "inherit"});
  fs.rmSync(tmp, {recursive: true, force: true});
  h.assertNoErrors(errors);
  await ctx.close();
}

h.run("screenshots", {repo: REPO, port: PORT}, async ({server, browser}) => {
  fs.mkdirSync(MEDIA, {recursive: true});
  await desktopStills(browser, server);
  await phone(browser, server);
  await workingsGif(browser, server);
});
