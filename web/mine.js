// agentviz-mine page: polls /api/agents, lays out the mine, eases between
// snapshots and hands the scene (scene/main.cpp) flat rows every frame.
"use strict";
(function () {
  const Q = new URLSearchParams(location.search);
  const FIXTURE = Q.get("fixture") === "1";
  const FEED = "/api/agents" + (FIXTURE ? "?fixture=1" : "");
  const POLL_MS = 2000;

  const STATES = {offline: 0, dim: 1, idle: 2, busy: 3, waiting: 4};
  const ACTIONS = {none: 0, edit: 1, run: 2, read: 3, web: 4, delegate: 5, think: 6, other: 7};
  const ICON = {edit: "⛏", run: "🛒", read: "🔦", web: "📡", delegate: "👷", think: "💭", other: "🪣", none: "·", resting: "💤"};
  const SHIRTS = ["#d94d38", "#1f9a94", "#d9a61f", "#8250c7", "#73b833", "#337ad9", "#cc4099", "#eb7a24"];
  const shirt = (seed) => SHIRTS[Math.min(7, Math.max(0, Math.floor(seed * 8)))];
  // Must match scene/main.cpp.
  const SHAFT_HALF = 0.8, CHAMBER_HALF = 1.9;
  const P_STRIDE = 6, S_STRIDE = 14, F_STRIDE = 12;

  const sessions = new Map();   // id -> entity
  const followers = new Map();  // sessionId/followerId -> entity
  const places = new Map();
  let world = {minX: -10, maxX: 10, bottom: -10};
  let doc = null, kit = null, mod = null, selected = null, fitted = false;
  const labelsEl = document.getElementById("labels");

  function ent(map, id, make) {
    let e = map.get(id);
    if (!e) { e = make(); e.id = id; map.set(id, e); e.el = null; }
    return e;
  }

  function label(e, cls, text, dotColor) {
    if (!e.el) { e.el = document.createElement("div"); labelsEl.appendChild(e.el); }
    const want = "label " + cls;
    if (e.el.className !== want) e.el.className = want;
    if (e.el._text !== text) {
      e.el._text = text;
      e.el.textContent = "";
      if (dotColor) {
        const d = document.createElement("span");
        d.className = "dot";
        d.style.background = dotColor;
        e.el.appendChild(d);
      }
      e.el.appendChild(document.createTextNode(text));
    }
  }

  // ------------------------------------------------------------ layout
  function layout(d) {
    const byPlace = new Map();
    for (const s of d.sessions) {
      if (!byPlace.has(s.place)) byPlace.set(s.place, []);
      byPlace.get(s.place).push(s);
    }
    const order = d.places.map((p) => p.id).filter((id) => byPlace.has(id));
    for (const id of byPlace.keys()) if (!order.includes(id)) order.push(id);
    let cursor = 0, bottom = -8;
    const seenS = new Set(), seenF = new Set(), seenP = new Set();
    const placeRows = [];
    for (const pid of order) {
      const meta = d.places.find((p) => p.id === pid) || {id: pid, label: pid, seed: 0.5};
      const list = byPlace.get(pid);
      let hx = null;
      const sessRows = [];
      for (const s of list) {
        const fl = (s.followers || []).map((f, k) => {
          const side = k % 2 === 0 ? 1 : -1, level = Math.floor(k / 2);
          return {f, side, floor: -4.3 - 2.7 * level, len: 1.4 + Math.min(3.8, 0.9 * Math.log2(1 + (f.toolCount || 0)))};
        });
        let left = CHAMBER_HALF, right = CHAMBER_HALF, lowest = 0;
        if (hx == null) { left = Math.max(left, 1.6); right = Math.max(right, 1.6); }
        for (const t of fl) {
          const ext = SHAFT_HALF + t.len + 0.3;
          if (t.side > 0) right = Math.max(right, ext); else left = Math.max(left, ext);
          lowest = Math.min(lowest, t.floor);
        }
        const x = cursor + left;
        cursor += left + right + 1.2;
        let face = -(5.6 + Math.min(2.5, Math.log2(1 + (s.uptimeSec || 0) / 900)));
        if (fl.length) face = Math.min(face, lowest - 2.8);
        if (s.state === "offline") face = -4.2;
        bottom = Math.min(bottom, face);
        sessRows.push({s, x, face, fl, under: hx == null});
        if (hx == null) hx = x;
      }
      cursor += 1.5;
      placeRows.push({meta, hx, sessRows});
    }
    const shift = -cursor / 2;
    world = {minX: shift, maxX: cursor + shift, bottom: bottom - 1.5};
    for (const pr of placeRows) {
      const p = ent(places, pr.meta.id, () => ({cur: {appear: 0}, tgt: {}}));
      seenP.add(p.id);
      p.data = pr.meta;
      p.tgt = {x: pr.hx + shift, appear: 1, waiting: pr.meta.waiting > 0 ? 1 : 0, busy: pr.meta.busy > 0 ? 1 : 0};
      if (p.cur.x == null) Object.assign(p.cur, p.tgt, {appear: 0});
      for (const r of pr.sessRows) {
        const s = r.s;
        const e = ent(sessions, s.id, () => ({cur: {appear: 0}, tgt: {}}));
        seenS.add(e.id);
        e.data = s;
        e.under = r.under;
        const lamp = {busy: 1, waiting: 0.8, idle: 0.25, dim: 0.15, offline: 0}[s.state] ?? 0.3;
        e.tgt = {x: r.x + shift, face: r.face, lamp, waiting: s.state === "waiting" ? 1 : 0,
          stalled: s.stalled ? 1 : 0, offline: s.state === "offline" ? 1 : 0, appear: 1};
        if (e.cur.x == null) Object.assign(e.cur, e.tgt, {appear: 0});
        for (const t of r.fl) {
          const fe = ent(followers, s.id + "/" + t.f.id, () => ({cur: {appear: 0, len: 0}, tgt: {}}));
          seenF.add(fe.id);
          fe.data = t.f;
          fe.parent = e;
          fe.tgt = {x: r.x + shift, y: t.floor, side: t.side, len: t.len, active: t.f.state === "active" ? 1 : 0, appear: 1};
          if (fe.cur.x == null) Object.assign(fe.cur, fe.tgt, {appear: 0, len: 0});
        }
      }
    }
    for (const [map, seen] of [[sessions, seenS], [followers, seenF], [places, seenP]])
      for (const e of map.values()) if (!seen.has(e.id)) { e.tgt.appear = 0; e.gone = true; if (e.tgt.len != null) e.tgt.len = 0; }
      else e.gone = false;
    if (!fitted && kit) { fit(true); fitted = true; }
  }

  // ------------------------------------------------------------ easing + rows
  function ease(e, k) {
    for (const key of Object.keys(e.tgt)) {
      const a = e.cur[key], b = e.tgt[key];
      if (typeof b !== "number") continue;
      e.cur[key] = a == null ? b : a + (b - a) * k;
    }
  }

  let bufPtr = 0, bufCap = 0;
  function upload(fn, rows, stride) {
    const n = rows.length / stride;
    if (rows.length > bufCap) {
      if (bufPtr) mod._free(bufPtr);
      bufCap = Math.max(rows.length, 1024) * 2;
      bufPtr = mod._malloc(bufCap * 4);
    }
    mod.HEAPF32.set(rows, bufPtr >> 2);
    fn(bufPtr, n);
  }

  function frame(t, now, dt) {
    const k = kit && kit.driven ? 1 : 1 - Math.exp(-(dt || 0.016) * 4);
    clampCamera();
    stepFocus(dt || 0.016);
    const pr = [], sr = [], fr = [];
    for (const [id, p] of places) {
      ease(p, k);
      if (p.gone && p.cur.appear < 0.02) { if (p.el) p.el.remove(); places.delete(id); continue; }
      pr.push(p.cur.x, 1.5, p.data.seed || 0, p.cur.waiting, p.cur.busy, p.cur.appear);
    }
    for (const [id, e] of sessions) {
      ease(e, k);
      if (e.gone && e.cur.appear < 0.02) { if (e.el) e.el.remove(); sessions.delete(id); continue; }
      const s = e.data, c = e.cur;
      sr.push(c.x, c.face, STATES[s.state] ?? 1, ACTIONS[s.action] ?? 0, s.seed || 0, c.lamp, c.waiting, c.stalled,
        c.offline, selected === e ? 1 : 0, c.appear, facingOf(e), e.under ? 1 : 0, 0);
    }
    for (const [id, f] of followers) {
      ease(f, k);
      if (f.gone && f.cur.len < 0.05) { if (f.el) f.el.remove(); followers.delete(id); continue; }
      const c = f.cur, p = f.parent;
      fr.push(p.cur.x, c.y, f.tgt.side, c.len, ACTIONS[f.data.action] ?? 0, c.active, f.data.seed || 0,
        selected === f ? 1 : 0, c.appear, p.data.seed || 0, p.cur.offline, 0);
    }
    mod._mn_set_world(world.minX, world.maxX, world.bottom);
    upload(mod._mn_set_places, new Float32Array(pr), P_STRIDE);
    upload(mod._mn_set_sessions, new Float32Array(sr), S_STRIDE);
    upload(mod._mn_set_followers, new Float32Array(fr), F_STRIDE);
  }

  // Positions the page uses for picking and labels.
  const minerPos = (e) => {
    const facing = facingOf(e);
    return {x: e.cur.x + facing * (CHAMBER_HALF - 0.85), y: e.cur.face};
  };
  const followerPos = (f) => ({x: f.parent.cur.x + f.tgt.side * (SHAFT_HALF + f.cur.len - 0.65), y: f.cur.y});
  // Which way a session's miner faces in its chamber (from its id's seed).
  const facingOf = (e) => (Math.floor((e.data.seed || 0) * 977) % 2 ? -1 : 1);

  function after() {
    if (!kit) return;
    const items = [];
    for (const p of places.values()) {
      label(p, "place", (p.data.label || p.id), null);
      items.push({el: p.el, q: kit.project(p.cur.x, 8.9, 0.1), rank: 1});
    }
    for (const e of sessions.values()) {
      const s = e.data;
      const cls = s.state === "waiting" ? "waiting" : s.state === "offline" ? "offline" : "";
      label(e, cls, (s.state === "waiting" ? "🔔 " : "") + s.name, shirt(s.seed || 0));
      items.push({el: e.el, q: kit.project(e.cur.x, 2.5, 0.2), rank: s.state === "waiting" || selected === e ? 0 : 2});
    }
    for (const f of followers.values()) {
      const showAll = selected && (selected === f || selected === f.parent || (selected.parent && selected.parent === f.parent));
      if (!showAll && cam.dist > 34) { if (f.el) f.el.style.display = "none"; continue; }
      label(f, "follower", (ICON[f.data.state === "resting" ? "resting" : f.data.action] || "") + " " + (f.data.label || "subagent"), null);
      const p = followerPos(f);
      items.push({el: f.el, q: kit.project(p.x, p.y + 2.0, 0.2), rank: showAll ? 1 : 4});
    }
    StrataKit.placeLabels(items);
  }

  // ------------------------------------------------------------ picking
  function candidates() {
    const out = [];
    for (const e of sessions.values()) {
      if (e.gone) continue;
      const m = minerPos(e);
      out.push({thing: e, pts: [[m.x, m.y + 0.9, 0.15], [e.cur.x, 1.0, 0.2], [e.cur.x, e.cur.face / 2, 0.0]]});
    }
    for (const f of followers.values()) {
      if (f.gone) continue;
      const m = followerPos(f);
      out.push({thing: f, pts: [[m.x, m.y + 0.7, 0.15]]});
    }
    return out;
  }
  function pick(x, y, radius) {
    let best = null, bestD = radius * radius * 2.2;
    for (const c of candidates())
      for (const p of c.pts) {
        const s = kit.project(p[0], p[1], p[2]);
        if (!s) continue;
        const d = (s.x - x) ** 2 + (s.y - y) ** 2;
        if (d < bestD) { bestD = d; best = c.thing; }
      }
    return best;
  }

  // ------------------------------------------------------------ panel
  const sheet = document.getElementById("sheet");
  const esc = (v) => String(v == null ? "" : v).replace(/[&<>"]/g, (c) => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;"}[c]));
  function renderSheet() {
    if (!selected || selected.gone) { sheet.hidden = true; return; }
    sheet.hidden = false;
    const isF = !!selected.parent;
    const d = selected.data;
    let html;
    if (!isF) {
      const st = d.stalled ? "stalled" : d.state;
      const act = d.action || "none";
      html = `<h2><span class="dot" style="display:inline-block;width:12px;height:12px;border-radius:50%;background:${shirt(d.seed || 0)}"></span>${esc(d.name)}
        <span class="state ${esc(st)}">${st === "waiting" ? "🔔 needs you" : esc(st)}</span><button class="close" aria-label="Close">×</button></h2>
        <div class="row"><span class="k">doing</span><span class="big">${ICON[act] || ""}</span><span>${esc(act)}</span></div>
        ${d.activity && d.activity.label ? `<div class="row"><span class="k">label</span><span>${esc(d.activity.tool)} · ${esc(d.activity.label)}</span></div>` : ""}
        <div class="row"><span class="k">place</span><span>🏗 ${esc(d.placeLabel || d.place)}</span></div>
        ${d.branch ? `<div class="row"><span class="k">branch</span><span>⎇ ${esc(d.branch)}</span></div>` : ""}
        <div class="row"><span class="k">tunnels</span><span>${(d.followers || []).length} subagent${(d.followers || []).length === 1 ? "" : "s"}</span></div>
        <ul>${(d.followers || []).map((f) => `<li data-f="${esc(d.id + "/" + f.id)}"><span class="big">${ICON[f.state === "resting" ? "resting" : f.action] || "·"}</span>
          <span>${esc(f.label || "subagent")}</span><span class="state ${esc(f.state)}">${esc(f.state)}</span><span class="n">${f.toolCount || 0} ⛏</span></li>`).join("")}</ul>`;
    } else {
      const p = selected.parent.data;
      const act = d.state === "resting" ? "resting" : d.action || "none";
      html = `<h2>${ICON[d.state === "resting" ? "resting" : d.action] || "·"} ${esc(d.label || "subagent")}<span class="state ${esc(d.state)}">${esc(d.state)}</span>
        <button class="close" aria-label="Close">×</button></h2>
        <div class="row"><span class="k">lead</span><span><span class="dot" style="display:inline-block;width:10px;height:10px;border-radius:50%;background:${shirt(p.seed || 0)}"></span> ${esc(p.name)}</span></div>
        <div class="row"><span class="k">doing</span><span>${esc(act)}</span></div>
        ${d.tool ? `<div class="row"><span class="k">tool</span><span>${esc(d.tool)}${d.toolLabel ? " · " + esc(d.toolLabel) : ""}</span></div>` : ""}
        <div class="row"><span class="k">depth</span><span>${d.toolCount || 0} tool calls</span></div>
        <div class="bar" style="width:${Math.min(100, 10 + 4 * (d.toolCount || 0))}%"></div>
        ${p.branch ? `<div class="row"><span class="k">branch</span><span>⎇ ${esc(p.branch)}</span></div>` : ""}`;
    }
    if (sheet._html !== html) {
      sheet._html = html;
      sheet.innerHTML = html;
      sheet.querySelector(".close").onclick = () => select(null);
      sheet.querySelectorAll("li[data-f]").forEach((li) => { li.onclick = () => select(followers.get(li.dataset.f) || null, true); });
    }
  }
  function select(thing, focusIt) {
    selected = thing;
    if (kit) kit.selected = thing;
    renderSheet();
    if (thing && focusIt) focusOn(thing);
  }

  // ------------------------------------------------------------ camera
  let focusAnim = null;
  const cam = {yaw: Math.PI / 2, pitch: 0.16, dist: 40, tx: 0, ty: -4, tz: 0};
  function clampCamera() {
    cam.yaw = Math.min(Math.max(cam.yaw, Math.PI / 2 - 0.9), Math.PI / 2 + 0.9);
    cam.tx = Math.min(Math.max(cam.tx, world.minX - 5), world.maxX + 5);
    cam.ty = Math.min(Math.max(cam.ty, world.bottom - 2), 8);
  }
  function fit(instant) {
    const canvas = document.getElementById("view");
    const aspect = Math.max(0.3, canvas.clientWidth / Math.max(1, canvas.clientHeight));
    const w = world.maxX - world.minX + 2, h = 9 - world.bottom;
    const tanH = Math.tan(25 * Math.PI / 180);
    let dist = Math.max(h / 2 / tanH, w / 2 / (tanH * aspect)) * 1.05;
    dist = Math.min(dist, aspect < 0.8 ? 70 : 150);
    const tgt = {tx: (world.minX + world.maxX) / 2, ty: (6 + world.bottom) / 2, dist, yaw: Math.PI / 2, pitch: 0.16};
    if (instant) Object.assign(cam, tgt); else focusAnim = {tgt, left: 1.2};
  }
  function focusOn(thing) {
    const p = thing.parent ? followerPos(thing) : minerPos(thing);
    const y = thing.parent ? p.y + 1 : (thing.data.state === "waiting" ? p.y / 2 + 1 : p.y + 1);
    const dist = Math.max(14, Math.min(cam.dist, 26));
    // On a phone the bottom sheet covers the lower part: aim below the subject so it sits above the sheet.
    const sheetShift = window.innerWidth <= 600 ? dist * 0.2 : 0;
    const tgt = {tx: p.x, ty: y - sheetShift, dist, yaw: Math.PI / 2, pitch: 0.16};
    if (kit && kit.driven) Object.assign(cam, tgt); else focusAnim = {tgt, left: 1.2};
  }
  function stepFocus(dt) {
    if (!focusAnim) return;
    const k = 1 - Math.exp(-dt * 5);
    for (const key of Object.keys(focusAnim.tgt)) cam[key] += (focusAnim.tgt[key] - cam[key]) * k;
    focusAnim.left -= dt;
    if (focusAnim.left <= 0) focusAnim = null;
  }

  // ------------------------------------------------------------ HUD + polling
  function renderHud() {
    if (!doc) return;
    const c = doc.counts || {};
    const waiting = doc.sessions.filter((s) => s.state === "waiting");
    document.getElementById("waitBtn").hidden = !waiting.length;
    document.getElementById("waitN").textContent = waiting.length;
    const stalled = doc.sessions.filter((s) => s.stalled).length;
    document.getElementById("counts").innerHTML =
      `<span title="busy">⛏ ${c.busy || 0}</span><span title="subagents" class="w">↔ ${c.activeFollowers || 0}/${c.followers || 0}</span>` +
      `<span title="idle" class="w">💤 ${c.idle || 0}</span>${stalled ? `<span title="stalled">🌫 ${stalled}</span>` : ""}` +
      `${doc.fixture ? '<span class="w">demo</span>' : ""}`;
  }
  let waitCycle = 0;
  document.getElementById("waitBtn").onclick = () => {
    const w = [...sessions.values()].filter((e) => e.data.state === "waiting" && !e.gone);
    if (!w.length) return;
    select(w[waitCycle++ % w.length], true);
  };
  document.getElementById("fitBtn").onclick = () => fit(false);
  const legendBtn = document.getElementById("legendBtn"), legend = document.getElementById("legend");
  legendBtn.onclick = () => { legend.hidden = !legend.hidden; legendBtn.setAttribute("aria-expanded", String(!legend.hidden)); };

  async function poll() {
    try {
      const r = await fetch(FEED, {cache: "no-store"});
      if (r.ok) {
        doc = await r.json();
        layout(doc);
        renderHud();
        if (selected && selected.gone) select(null);
        else renderSheet();
      }
    } catch (e) { /* server restarting: keep the last snapshot */ }
    setTimeout(poll, POLL_MS);
  }

  // ------------------------------------------------------------ boot
  window.__mine = {sessions, followers, places, select, focusOn, fit, get selected() { return selected; }, cam,
    look: (o) => Object.assign(cam, o),
    posOf: (th) => (th.parent ? followerPos(th) : minerPos(th)),
    findSession: (name) => [...sessions.values()].find((e) => e.data.name === name) || null,
    findFollower: (lbl) => [...followers.values()].find((f) => f.data.label === lbl) || null};

  (async () => {
    try {
      mod = await StrataLoader.load({script: "mine-engine.js", factory: "createMine", wasm: "mine-engine.wasm",
        title: "the mine"});
    } catch (e) { return; }
    const canvas = document.getElementById("view");
    try {
      kit = StrataKit.create({
        module: mod, canvas, camera: cam, idleOrbit: false,
        limits: {minPitch: -0.2, maxPitch: 1.1, minDist: 6, maxDist: 220},
        frame, after, pick,
        idOf: (th) => th.id,
        onTap(th) { select(th || null, false); },
        onSlow: () => StrataLoader.slow(),
        onFatal: (code, msg) => StrataLoader.fallback(code, msg),
        targets: () => candidates().map((c) => {
          const s = kit.project(c.pts[0][0], c.pts[0][1], c.pts[0][2]);
          const r = canvas.getBoundingClientRect();
          return s && {id: c.thing.id, x: s.x + r.left, y: s.y + r.top};
        }).filter(Boolean),
      });
    } catch (e) {
      return StrataLoader.fallback(e.code === "no-webgl2" ? "no-webgl2" : "engine");
    }
    kit.setSun(Date.UTC(2026, 5, 21, 17, 0, 0) / 1000, 40, -105);
    if (doc) { fit(true); fitted = true; }
    kit.start();
    if (kit.driven) kit.drive(0);
    StrataLoader.done();
    canvas.focus({preventScroll: true});
  })();
  poll();
})();
