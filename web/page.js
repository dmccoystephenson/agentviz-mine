/* page.js: boots the mine. The feed client, HUD, labels and wire come from
   the agentviz kit (agentviz-*.js); this file wires them to the Mine model
   (mine-model.js) and the Strata kit, and owns the camera. */
(function () {
  "use strict";
  const HALF_PI = Math.PI / 2, PITCH = 0.16, FOV_HALF_TAN = Math.tan(25 * Math.PI / 180);
  const REASON = {question: "has a question", permission: "needs permission"};
  // The scene's shirt colours (scene/draw_util.h kShirts), for the label edges.
  const SHIRTS = ["#d94d38", "#1f9a94", "#d9a61f", "#8250c7", "#73b833", "#337ad9", "#cc4099", "#eb7a24"];
  const shirtOf = (seed) => SHIRTS[Math.min(7, Math.max(0, Math.floor((seed || 0) * 8)))];
  const FOLLOWER_LABELS_WITHIN = 34;
  const canvas = document.getElementById("view");
  const mine = new window.Mine();
  const wire = AgentvizWire.create();
  const labels = AgentvizScreen.labels(document.getElementById("labels"));
  const cam = {yaw: HALF_PI, pitch: PITCH, dist: 40, tx: 0, ty: -4, tz: 0};
  let kit = null, mod = null, feed = null, pulseOverride = null, selectedId = null, hoveredId = null, camAnim = null, fitted = false;

  /* ------------------------------------------------------------ camera */
  function clampCamera() {
    const w = mine.world;
    cam.yaw = Math.min(Math.max(cam.yaw, HALF_PI - 0.9), HALF_PI + 0.9);
    cam.tx = Math.min(Math.max(cam.tx, w.minX - 5), w.maxX + 5);
    cam.ty = Math.min(Math.max(cam.ty, w.bottom - 2), 8);
  }
  // The whole cross-section, from the headframes' tops to the deepest face.
  function fitView() {
    const w = mine.world;
    const aspect = Math.max(0.3, canvas.clientWidth / Math.max(1, canvas.clientHeight));
    const width = w.maxX - w.minX + 2, height = 9 - w.bottom;
    const dist = Math.min(Math.max(height / 2 / FOV_HALF_TAN, width / 2 / (FOV_HALF_TAN * aspect)) * 1.05,
      aspect < 0.8 ? 70 : 150);
    // A wide mine leaves height to spare: keep the deepest face near the bottom
    // edge and give the spare to the sky, not to bare bedrock.
    const ty = Math.max((6 + w.bottom) / 2, w.bottom - 1.5 + dist * FOV_HALF_TAN * 0.92);
    // With height to spare, look down a little more: the land behind the cut
    // fills the upper part of the view instead of bare sky.
    const spare = dist * FOV_HALF_TAN * 2 - height;
    const lift = Math.min(0.2, Math.max(0, spare / dist) * 0.5);
    return {tx: (w.minX + w.maxX) / 2, ty: Math.min(ty, 4), tz: 0, dist, yaw: HALF_PI, pitch: PITCH + lift};
  }
  function flyTo(to, instant) {
    if (instant || !kit || kit.driven) { Object.assign(cam, to); camAnim = null; return; }
    camAnim = {from: Object.assign({}, cam), to, t0: performance.now(), ms: 900};
  }
  function fit(instant) { if (mine.ready) flyTo(fitView(), instant); }
  function stepCam() {
    if (!camAnim) return;
    const k = Math.min(1, (performance.now() - camAnim.t0) / camAnim.ms), e = AgentvizFeed.smoothstep(k);
    for (const key of Object.keys(camAnim.to)) cam[key] = AgentvizFeed.lerp(camAnim.from[key], camAnim.to[key], e);
    if (k >= 1) camAnim = null;
  }
  function focusOn(e) {
    const p = mine.posOf(e);
    const waiting = !e.parent && e.data.state === "waiting";
    const y = waiting ? p.y / 2 + 1 : p.y + 1;
    const dist = Math.max(14, Math.min(cam.dist, 26));
    // On a phone the bottom sheet covers the lower part: aim below the subject so it sits above the sheet.
    const sheetShift = window.innerWidth <= 640 ? dist * 0.2 : 0;
    flyTo({tx: p.x, ty: y - sheetShift, tz: 0, dist, yaw: HALF_PI, pitch: PITCH});
  }

  /* ------------------------------------------------------------ labels */
  function edge(el, colour) { if (el.style.borderColor !== colour) el.style.borderColor = colour; }

  // A label's state at a glance: "needs you" is solid red, a stall gets an
  // amber edge, idle and offline sessions recede so the busy ones stand out.
  function sessionLabel(e) {
    const s = e.data, waiting = s.state === "waiting";
    const cls = "lead" + (waiting ? " waiting" : s.stalled ? " stalled" : s.state === "offline" ? " offline"
      : s.state === "busy" ? "" : " quiet");
    const text = waiting ? (s.name || s.id) + " " + (REASON[s.waitingReason] || "needs you") : s.name || s.id.slice(0, 6);
    const el = labels.set("s:" + e.id, {cls, state: s.stalled ? "stalled" : s.state, text});
    edge(el, waiting || s.stalled ? "" : shirtOf(s.seed));
    return {el, q: kit.project(e.cur.x, 2.5, 0.2), rank: waiting || e.id === selectedId ? 0 : 2};
  }

  // Subagent labels show close up, or for the selected agent's family.
  function followerLabel(f) {
    const sel = selectedId && mine.get(selectedId);
    const family = sel && (sel === f || sel === f.parent || sel.parent === f.parent);
    if (!family && cam.dist > FOLLOWER_LABELS_WITHIN) return null;
    const d = f.data, resting = d.state !== "active";
    const el = labels.set("f:" + f.id, {cls: "sub" + (d.stalled ? " stalled" : resting ? " quiet" : ""),
      state: d.stalled ? "stalled" : null, action: resting ? "none" : d.action, text: d.label || "subagent"});
    edge(el, d.stalled ? "" : shirtOf(f.session.seed));
    const p = mine.followerPos(f);
    return {el, q: kit.project(p.x, p.y + 2.0, 0.2), rank: family ? 1 : 4};
  }

  function placeLabels() {
    const items = [];
    labels.begin();
    for (const p of mine.places.values()) {
      if (p.cur.appear < 0.5) continue;
      items.push({el: labels.set("p:" + p.id, {cls: "place", text: p.data.label || p.id}), q: kit.project(p.cur.x, 8.9, 0.1), rank: 1});
    }
    for (const e of mine.sessions.values()) if (!e.gone) items.push(sessionLabel(e));
    for (const f of mine.followers.values()) {
      const item = !f.gone && followerLabel(f);
      if (item) items.push(item);
    }
    labels.end();
    clearOfHud(items);
    StrataKit.placeLabels(items);
  }

  // World labels never sit under the top bar: a "needs you" label slides down
  // below it, any other label hides until the camera moves it clear.
  let hudRects = [], hudRectsAt = -1e9;
  function clearOfHud(items) {
    const now = performance.now();
    if (now - hudRectsAt > 300) {
      hudRectsAt = now;
      hudRects = [];
      const bar = document.querySelector(".av-bar");
      // The chips themselves: with oneRow the counts sit in .av-bar-scroll
      // (display: contents on a desktop, so it has no box of its own there).
      for (const c of bar ? bar.querySelectorAll(":scope > *, .av-bar-scroll > *") : []) {
        const r = c.getBoundingClientRect();
        if (r.width > 0 && !c.classList.contains("av-spacer")) hudRects.push(r);
      }
    }
    const low = hudRects.reduce((m, r) => Math.max(m, r.bottom), 0) + 60;
    for (const it of items) {
      if (!it.q || it.q.y > low) continue;
      const w = it.el.offsetWidth || 80, h = it.el.offsetHeight || 20, x0 = it.q.x - w / 2, x1 = it.q.x + w / 2;
      for (const r of hudRects) {
        if (x1 < r.left - 4 || x0 > r.right + 4 || it.q.y - h > r.bottom + 4) continue;
        it.q = it.rank <= 0 ? {x: it.q.x, y: r.bottom + 6 + h, w: it.q.w} : null;
        if (!it.q) break;
      }
    }
  }

  /* ------------------------------------------------------------ picking + selection */
  function candidates() {
    const out = [];
    for (const e of mine.sessions.values()) {
      if (e.gone) continue;
      const m = mine.minerPos(e);
      out.push({item: e, points: [[m.x, m.y + 0.9, 0.15], [e.cur.x, 1.0, 0.2], [e.cur.x, e.cur.face / 2, 0.0]]});
    }
    for (const f of mine.followers.values()) {
      if (f.gone) continue;
      const m = mine.followerPos(f);
      out.push({item: f, points: [[m.x, m.y + 0.7, 0.15]]});
    }
    return out;
  }
  const pick = (x, y, radius) => AgentvizScreen.pick(kit.project, candidates(), x, y, radius * 1.5);
  const targetOf = (e) => (e ? {session: e.session, follower: e.follower} : null);
  const idOf = (target) => (target ? (target.follower ? AgentvizFeed.followerKey(target.session, target.follower) : target.session.id) : null);

  function onSelect(target, how) {
    selectedId = idOf(target);
    const e = selectedId && mine.get(selectedId);
    if (kit) kit.selected = e || null;
    // On a phone a tap also lifts the subject above the bottom sheet, so the
    // card never covers what was tapped.
    if (e && (how === "waiting" || how === "list" || (how === "tap" && window.innerWidth <= 640))) focusOn(e);
  }

  const hud = AgentvizHud.create({
    title: "⛏ agentviz mine",
    oneRow: true,
    legend: document.getElementById("legend"),
    buttons: [{icon: "home", label: "Show the whole mine", onClick: () => fit(false)}],
    rows: ({session, follower}) => (follower ? [] : [["tool calls", session.toolCount != null ? session.toolCount : "—"]]),
    onSelect,
  });

  function accept(doc) {
    mine.update(doc);
    hud.update(doc);
    if (!fitted && kit) { fitted = true; fit(true); }
  }

  /* ------------------------------------------------------------ boot */
  function frame(t, now, dt) {
    stepCam();
    clampCamera();
    mine.step(dt || 0.016, kit.driven);
    if (!mine.ready) return;
    const highlight = (id) => (id === selectedId ? 1 : id === hoveredId ? 2 : 0);
    window.MineFrame.write(wire, mine, feed.doc, highlight, pulseOverride || feed.pulse).send(mod);
  }

  function screenTargets() {
    const r = canvas.getBoundingClientRect();
    return candidates().map((c) => {
      const s = kit.project(c.points[0][0], c.points[0][1], c.points[0][2]);
      return s && {id: c.item.id, x: s.x + r.left, y: s.y + r.top};
    }).filter(Boolean);
  }

  function createKit() {
    return StrataKit.create({
      module: mod, canvas, camera: cam, idleOrbit: false,
      limits: {minPitch: -0.2, maxPitch: 1.1, minDist: 6, maxDist: 220},
      frame,
      after: () => { if (mine.ready) placeLabels(); },
      pick,
      idOf: (e) => e.id,
      onTap: (e) => hud.select(targetOf(e), "tap"),
      onHover(e) { hoveredId = e ? e.id : null; canvas.style.cursor = e ? "pointer" : ""; },
      onSlow: () => StrataLoader.slow(),
      onFatal: (code, msg) => StrataLoader.fallback(code, msg),
      targets: screenTargets,
    });
  }

  async function boot() {
    try {
      mod = await StrataLoader.load({script: "mine-engine.js", factory: "createMine", wasm: "mine-engine.wasm", title: "the mine"});
    } catch (error) {
      console.error("mine: the engine did not load", error);
      return;  // StrataLoader has already shown its fallback
    }
    try {
      kit = createKit();
    } catch (error) {
      StrataLoader.fallback(error.code === "no-webgl2" ? "no-webgl2" : "engine");
      return;
    }
    feed = AgentvizFeed.create({onUpdate: accept, onStatus: hud.setStatus});
    window.__mine = mine;
    window.__page = {kit, mine, hud, cam, fit, look: (o) => flyTo(o, true),
      select: (id, focus) => hud.select(targetOf(mine.get(id)), focus ? "list" : "api"),
      get selectedId() { return selectedId; }, get doc() { return feed.doc; }, get polls() { return feed.polls; },
      stopPolling: () => feed.stop(),  // holdPulse: captures freeze a tool-call burst at a chosen strength
      holdPulse: (fn) => { pulseOverride = fn; },
      pollOnce: () => feed.pollOnce(), feed: (d) => feed.push(d)};
    kit.setSun(Date.UTC(2026, 5, 21, 17, 0, 0) / 1000, 40, -105);  // always midday: the cut face reads best
    feed.start();
    kit.start();
    if (kit.driven) kit.drive(0);
    StrataLoader.done();
    canvas.focus({preventScroll: true});
  }

  boot();
})();
