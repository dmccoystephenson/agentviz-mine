/* mine-frame.js: packs the mine for the scene, once a frame, in the agentviz
   wire format (vendor/agentviz-kit, docs/KIT.md in agentviz). scene/main.cpp
   and its headers read what is written here.

     scene floats  world min x, max x, deepest floor (negative y)
     zones         kind 0 pithead (one per place); extras: headframe x, waiting, busy
     agents        sessions at their shaft (x) and face (y), facing +1/-1;
                     extras: lamp, waiting, stalled, offline, under the headframe
                   followers at their shaft (x) and tunnel floor (y), facing = side;
                     extras: tunnel length, active, stalled, lead offline
     links         none

   Classic script: window.MineFrame. */
(function () {
  "use strict";
  const ZONE = {pithead: 0};
  const HEADFRAME_HALF = 1.3;

  function pithead(wire, p) {
    const x = p.cur.x;
    wire.zone({id: "place:" + p.id, kind: ZONE.pithead, seed: p.data.seed || 0, appear: p.cur.appear,
      x0: x - HEADFRAME_HALF, z0: 0, x1: x + HEADFRAME_HALF, z1: 0, extra: [x, p.cur.waiting, p.cur.busy]});
  }

  function lead(wire, mine, e, highlight, pulse) {
    const c = e.cur;
    wire.agent({session: e.session, x: c.x, y: c.face, z: 0, facing: mine.facingOf(e), appear: c.appear, highlight, pulse,
      extra: [c.lamp, c.waiting, c.stalled, c.offline, e.underHeadframe ? 1 : 0]});
  }

  function subagent(wire, f, highlight, pulse) {
    const c = f.cur;
    wire.agent({session: f.session, follower: f.follower, x: f.parent.cur.x, y: c.y, z: 0, facing: f.side,
      appear: c.appear, highlight, pulse, extra: [c.len, c.active, c.stalled, f.parent.cur.offline]});
  }

  // highlightOf(id) -> 0 none, 1 selected, 2 hovered; pulseOf(id) -> 0..1.
  function write(wire, mine, doc, highlightOf, pulseOf) {
    const w = mine.world;
    wire.begin({doc, scene: [w.minX, w.maxX, w.bottom]});
    for (const p of mine.places.values()) pithead(wire, p);
    for (const e of mine.sessions.values()) lead(wire, mine, e, highlightOf(e.id), pulseOf(e.id));
    for (const f of mine.followers.values()) subagent(wire, f, highlightOf(f.id), pulseOf(f.id));
    return wire;
  }

  window.MineFrame = {write, ZONE};
})();
