// Headless smoke test: serve the fixture, load /?fixture=1, wait for the kit
// to draw, check the frame is not flat, the waiting bell is up, a tap on the
// waiting miner opens the panel, and nothing errs; save docs/media/screenshot.png.
//   NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs
const path = require("path");
const {ROOT, startServer, open, launch} = require("./harness.cjs");

(async () => {
  const server = await startServer();
  const browser = await launch();
  let failed = false;
  const fail = (m) => { console.error("FAIL:", m); failed = true; };
  try {
    const {page, errors} = await open(browser);
    await page.waitForTimeout(3000);
    const st = await page.evaluate(() => window.__strataKit.stats());
    if (st.primitives < 200) fail(`too few primitives drawn: ${st.primitives}`);
    const n = await page.evaluate(() => [window.__mine.sessions.size, window.__mine.followers.size, window.__mine.places.size]);
    if (n[0] !== 8 || n[1] !== 11 || n[2] !== 5) fail(`expected 8 sessions, 11 subagents, 5 places; got ${n}`);
    const shot = await page.screenshot({path: path.join(ROOT, "docs/media/screenshot.png"), timeout: 180000});
    const lit = await page.evaluate(() => {
      const c = document.getElementById("view"), g = c.getContext("webgl2");
      const w = g.drawingBufferWidth, h = g.drawingBufferHeight, px = new Uint8Array(w * h * 4);
      g.readPixels(0, 0, w, h, g.RGBA, g.UNSIGNED_BYTE, px);
      const seen = new Set();
      for (let i = 0; i < px.length; i += 4 * 97) seen.add((px[i] >> 4) * 256 + (px[i + 1] >> 4) * 16 + (px[i + 2] >> 4));
      return seen.size;
    });
    if (lit < 20) fail(`frame looks flat (${lit} colours)`);
    if (await page.isHidden("#waitBtn")) fail("the waiting bell is not shown");
    const targets = await page.evaluate(() => window.__strataKit.targets());
    const t = targets.find((x) => x.id === "demo-waiting");
    if (!t) fail("no tap target for the waiting miner");
    else {
      await page.mouse.click(t.x, t.y);
      await page.waitForTimeout(500);
      const panel = await page.textContent("#sheet");
      if (!/Thistle/.test(panel || "") || !/needs you/.test(panel || "")) fail(`panel after tap: ${panel}`);
    }
    if (errors.length) fail("console errors: " + errors.join(" | "));
    console.log(failed ? "smoke: FAIL" : `smoke: OK (${st.primitives} primitives, ${st.points} points, ${st.lines} lines, ${shot.length} B png)`);
  } catch (e) {
    fail(e.stack || String(e));
  } finally {
    await browser.close();
    server.kill();
  }
  process.exit(failed ? 1 : 0);
})();
