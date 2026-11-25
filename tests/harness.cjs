// Shared by smoke.cjs and screenshots.cjs: serve the fixture feed on the test
// port (8945) and open headless Chromium (SwiftShader) on /?fixture=1.
//   NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs
const {spawn} = require("child_process");
const path = require("path");
const http = require("http");

function loadPlaywright() {
  for (const where of ["playwright", "/root/arcade-watch/node_modules/playwright", "/root/casework/node_modules/playwright"]) {
    try { return require(where); } catch (e) { /* next */ }
  }
  throw new Error("Playwright not found: set NODE_PATH to a node_modules that has it");
}
const {chromium} = loadPlaywright();
const ROOT = path.resolve(__dirname, "..");
const PORT = Number(process.env.MINE_TEST_PORT || 8945);

const get = (url) => new Promise((resolve) => {
  http.get(url, (r) => { r.resume(); resolve(r.statusCode); }).on("error", () => resolve(0));
});

async function startServer() {
  const proc = spawn("python3", ["-m", "vendor.agentfeed", "serve", "--web", "web", "--port", String(PORT), "--fixture"],
    {cwd: ROOT, stdio: ["ignore", "ignore", "inherit"]});
  for (let i = 0; i < 100; i++) {
    if ((await get(`http://127.0.0.1:${PORT}/api/agents`)) === 200) return proc;
    await new Promise((r) => setTimeout(r, 100));
  }
  proc.kill();
  throw new Error("server did not start");
}

async function open(browser, opts = {}) {
  const ctx = await browser.newContext({viewport: opts.viewport || {width: 1280, height: 800},
    deviceScaleFactor: opts.dpr || 1, isMobile: !!opts.mobile, hasTouch: !!opts.mobile});
  const page = await ctx.newPage();
  const errors = [];
  page.on("console", (m) => { if (m.type() === "error") errors.push(m.text()); });
  page.on("pageerror", (e) => errors.push(String(e)));
  // The kit's frame loop keeps `load` from firing: wait for the response only.
  await page.goto(`http://127.0.0.1:${PORT}/?fixture=1${opts.query || ""}`, {waitUntil: "commit"});
  await page.waitForFunction(() => window.__mine && window.__mine.sessions.size > 0 && window.__strataKit &&
    window.__strataKit.stats && (location.search.includes("drive=1") || (window.__strataKit.stats() || {}).frames > 3), null, {timeout: 120000, polling: 250});
  return {ctx, page, errors};
}

const launch = () => chromium.launch({args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"]});

module.exports = {ROOT, PORT, startServer, open, launch};
