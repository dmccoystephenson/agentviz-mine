// workings.h: the mine's reading of one wire frame (web/mine-frame.js packs it).
//   scene floats  world min x, max x, deepest floor
//   zones         pitheads; extras: headframe x, waiting, busy
//   agents        sessions (a shaft and its miner) and followers (a side tunnel)
#pragma once

#include "../vendor/agentviz-kit/scene/agentviz.h"
#include "draw_util.h"

namespace mine {

struct World {
    float minX, maxX, bottom;
};

struct Pithead {
    float x, seed, waiting, busy, appear;
};

// A session: its shaft at x, its chamber floor (the face) at y = face.
struct Lead {
    float x, face, facing, seed, lamp, waiting, stalled, offline, appear, pulse;
    bool underHeadframe, selected;
    agentviz::State state;
    agentviz::Action action;
    agentviz::WaitingReason reason;
};

// A subagent: a tunnel off its lead's shaft (at sx) on one side, floor at y.
struct Tunnel {
    float sx, y, side, len, active, stalled, leadOffline, seed, leadSeed, appear, pulse;
    bool selected;
    agentviz::Action action;
};

inline World worldOf(const agentviz::Frame& f) { return {f.scene[0], f.scene[1], f.scene[2]}; }

inline Pithead pitheadOf(const agentviz::Zone& z) {
    return {z.extra[0], z.seed, z.extra[1], z.extra[2], z.appear};
}

inline Lead leadOf(const agentviz::Agent& a) {
    return {a.x, a.y, a.facing < 0.0f ? -1.0f : 1.0f, a.seed, a.extra[0], a.extra[1], a.extra[2], a.extra[3],
            a.appear, a.pulse, a.extra[4] > 0.5f, a.highlight == agentviz::Highlight::Selected,
            a.state, a.action, a.waitingReason};
}

inline Tunnel tunnelOf(const agentviz::Agent& a) {
    return {a.x, a.y, a.facing < 0.0f ? -1.0f : 1.0f, a.extra[0], a.extra[1], a.extra[2], a.extra[3], a.seed,
            a.parentSeed, a.appear, a.pulse, a.highlight == agentviz::Highlight::Selected, a.action};
}

// The x of a tunnel's far end (its working face).
inline float tunnelEnd(const Tunnel& t) { return t.sx + t.side * (kShaftHalf + t.len); }

}  // namespace mine
