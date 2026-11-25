/* mine-model.js: lays the mine out from a feed document and eases it.

   One pithead per place, one shaft per session (a chamber at the bottom, the
   miner at its face), one side tunnel per subagent off its lead's shaft. A
   tunnel grows with the subagent's tool calls; a place's shafts are sunk
   deeper the more tool calls its sessions have made (the place's toolCount).
   Everything is keyed by id and eased towards the latest snapshot, so a poll
   never rebuilds the scene.

   Classic script: window.Mine. Needs AgentvizFeed. */
(function () {
  "use strict";
  const {followerKey} = window.AgentvizFeed;

  // Shapes the scene draws (scene/draw_util.h): keep in step.
  const SHAFT_HALF = 0.8, CHAMBER_HALF = 1.9;
  const FIRST_LEVEL = -4.3, LEVEL_GAP = 2.7;
  const SHAFT_GAP = 1.2, PLACE_GAP = 1.5;
  const OFFLINE_FACE = -4.2, SURFACE_MARGIN = 1.5;
  const EASE_RATE = 4;
  const LAMP = {busy: 1, waiting: 0.8, idle: 0.25, dim: 0.15, offline: 0};

  // Subagent tunnels alternate right and left, two to a level, going down.
  function tunnelsOf(session) {
    return (session.followers || []).map((follower, k) => ({
      follower,
      side: k % 2 === 0 ? 1 : -1,
      floor: FIRST_LEVEL - LEVEL_GAP * Math.floor(k / 2),
      len: 1.4 + Math.min(3.8, 0.9 * Math.log2(1 + (follower.toolCount || 0))),
    }));
  }

  // How far the workings reach either side of the shaft's centre line.
  function reachOf(tunnels) {
    let left = CHAMBER_HALF, right = CHAMBER_HALF;
    for (const t of tunnels) {
      const extent = SHAFT_HALF + t.len + 0.3;
      if (t.side > 0) right = Math.max(right, extent); else left = Math.max(left, extent);
    }
    return {left, right};
  }

  // Busier places have been mined deeper: 5.6 to 8.1 below the surface.
  const placeDepth = (place) => 5.6 + Math.min(2.5, Math.log2(1 + (place.toolCount || 0) / 40));

  function faceOf(session, place, tunnels) {
    if (session.state === "offline") return OFFLINE_FACE;
    const lowest = tunnels.reduce((y, t) => Math.min(y, t.floor), 0);
    const face = -placeDepth(place);
    return tunnels.length ? Math.min(face, lowest - 2.8) : face;
  }

  // Places in the feed's order, then any place a session names but the list missed.
  function groupByPlace(doc) {
    const byPlace = new Map();
    for (const s of doc.sessions || []) {
      if (!byPlace.has(s.place)) byPlace.set(s.place, []);
      byPlace.get(s.place).push(s);
    }
    const known = new Map((doc.places || []).map((p) => [p.id, p]));
    const order = (doc.places || []).map((p) => p.id).filter((id) => byPlace.has(id));
    for (const id of byPlace.keys()) if (!order.includes(id)) order.push(id);
    return order.map((id) => ({place: known.get(id) || {id, label: id, seed: 0.5}, sessions: byPlace.get(id)}));
  }

  // Shaft x positions left to right, from 0; recentred by the caller.
  function planShafts(groups) {
    let cursor = 0, bottom = -8;
    const plan = [];
    for (const {place, sessions} of groups) {
      const shafts = sessions.map((session, i) => {
        const tunnels = tunnelsOf(session), reach = reachOf(tunnels);
        const x = cursor + reach.left;
        cursor += reach.left + reach.right + SHAFT_GAP;
        const face = faceOf(session, place, tunnels);
        bottom = Math.min(bottom, face);
        return {session, x, face, tunnels, underHeadframe: i === 0};
      });
      cursor += PLACE_GAP;
      plan.push({place, x: shafts[0].x, shafts});
    }
    return {plan, width: cursor, bottom};
  }

  class Mine {
    constructor() {
      this.places = new Map();     // place id -> entity
      this.sessions = new Map();   // session id -> entity
      this.followers = new Map();  // "session/follower" -> entity
      this.world = {minX: -10, maxX: 10, bottom: -10};
      this.ready = false;
    }

    // An entity per id; `cur` eases towards `tgt`. New ones start invisible.
    _entity(map, id, seen, tgt, extra) {
      let e = map.get(id);
      if (!e) { e = {id, cur: Object.assign({}, tgt, {appear: 0}, extra && extra.start)}; map.set(id, e); }
      seen.add(id);
      e.tgt = tgt;
      e.gone = false;
      return Object.assign(e, extra && extra.data);
    }

    update(doc) {
      const {plan, width, bottom} = planShafts(groupByPlace(doc));
      const shift = -width / 2;
      this.world = {minX: shift, maxX: width + shift, bottom: bottom - SURFACE_MARGIN};
      const seen = {places: new Set(), sessions: new Set(), followers: new Set()};
      for (const row of plan) {
        const p = row.place;
        this._entity(this.places, p.id, seen.places, {x: row.x + shift, appear: 1,
          waiting: p.waiting > 0 ? 1 : 0, busy: p.busy > 0 ? 1 : 0}, {data: {data: p}});
        for (const shaft of row.shafts) this._placeShaft(shaft, shift, seen);
      }
      this._retire(this.places, seen.places);
      this._retire(this.sessions, seen.sessions);
      this._retire(this.followers, seen.followers);
      this.ready = true;
    }

    _placeShaft(shaft, shift, seen) {
      const s = shaft.session, x = shaft.x + shift;
      const lead = this._entity(this.sessions, s.id, seen.sessions, {x, face: shaft.face, lamp: LAMP[s.state] != null ? LAMP[s.state] : 0.3,
        waiting: s.state === "waiting" ? 1 : 0, stalled: s.stalled ? 1 : 0, offline: s.state === "offline" ? 1 : 0, appear: 1},
      {data: {data: s, session: s, follower: null, underHeadframe: shaft.underHeadframe}});
      for (const t of shaft.tunnels) {
        this._entity(this.followers, followerKey(s, t.follower), seen.followers,
          {x, y: t.floor, len: t.len, active: t.follower.state === "active" ? 1 : 0, stalled: t.follower.stalled ? 1 : 0, appear: 1},
          {start: {len: 0}, data: {data: t.follower, session: s, follower: t.follower, parent: lead, side: t.side}});
      }
    }

    // Things that left the feed fade (and tunnels close) before they are dropped.
    _retire(map, seen) {
      for (const e of map.values()) {
        if (seen.has(e.id)) continue;
        e.gone = true;
        e.tgt = Object.assign({}, e.tgt, {appear: 0}, e.tgt.len != null ? {len: 0} : {});
      }
    }

    step(dt, instant) {
      const k = instant ? 1 : 1 - Math.exp(-dt * EASE_RATE);
      for (const map of [this.places, this.sessions, this.followers]) {
        for (const [id, e] of map) {
          for (const key of Object.keys(e.tgt)) e.cur[key] = e.cur[key] == null ? e.tgt[key] : e.cur[key] + (e.tgt[key] - e.cur[key]) * k;
          const faded = e.cur.len != null ? e.cur.len < 0.05 : e.cur.appear < 0.02;
          if (e.gone && faded) map.delete(id);
        }
      }
    }

    // Which way a session's miner faces in its chamber (from its seed).
    facingOf(e) { return Math.floor((e.data.seed || 0) * 977) % 2 ? -1 : 1; }
    minerPos(e) { return {x: e.cur.x + this.facingOf(e) * (CHAMBER_HALF - 0.85), y: e.cur.face}; }
    followerPos(f) { return {x: f.parent.cur.x + f.side * (SHAFT_HALF + f.cur.len - 0.65), y: f.cur.y}; }
    posOf(e) { return e.parent ? this.followerPos(e) : this.minerPos(e); }
    get(id) { return this.sessions.get(id) || this.followers.get(id) || null; }
    findSession(name) { return [...this.sessions.values()].find((e) => e.data.name === name) || null; }
    findFollower(label) { return [...this.followers.values()].find((f) => f.data.label === label) || null; }
  }

  window.Mine = Mine;
})();
