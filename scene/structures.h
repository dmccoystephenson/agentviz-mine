// structures.h: what is built round the workings: headframes over the
// places, shaft timbering and ladders, tunnel props and rails, the boarded-up
// shafts of offline sessions, the red cage of a session waiting on the owner
// (with a "?" or a padlock saying why) and the dust and amber lamp of a stall.
#pragma once

#include "workings.h"

namespace mine {

const Rgb kLadder{0.45f, 0.3f, 0.15f};
const Rgb kCageBar{0.75f, 0.12f, 0.08f};

// The sheave wheel on top, turning while anyone below is busy.
inline void drawSheave(Host& h, float x, float wy, float z, float spin) {
    ring(h, x, wy, z + 0.05f, 0.7f, {0.2f, 0.2f, 0.22f}, 1.0f, 24);
    for (int k = 0; k < 4; ++k) {
        const float a = spin + k * kPi / 4.0f;
        stick(h, x - std::cos(a) * 0.68f, wy - std::sin(a) * 0.68f, x + std::cos(a) * 0.68f, wy + std::sin(a) * 0.68f,
              z + 0.05f, {0.25f, 0.25f, 0.28f}, 0.05f);
    }
}

// The winding house behind the headframe, smoking while busy.
inline void drawWindingHouse(Host& h, float x, float seed, float busy, float t) {
    h.add(STRATA_PRIMITIVE_CUBE, x, 0.75f, kBack - 1.8f, 2.6f, 1.5f, 1.8f, {0.55f, 0.42f, 0.32f});
    h.add(STRATA_PRIMITIVE_CUBE, x, 1.6f, kBack - 1.8f, 2.9f, 0.25f, 2.1f, {0.3f, 0.16f, 0.12f});
    h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.9f, 2.2f, kBack - 2.2f, 0.3f, 0.7f, 0.3f, {0.35f, 0.25f, 0.2f});
    if (busy <= 0.05f) return;
    for (int k = 0; k < 5; ++k) {
        const float ph = frac(t * 0.25f + k / 5.0f + seed);
        h.point(x + 0.9f + ph * 1.2f, 2.9f + ph * 2.5f, kBack - 2.2f, 6.0f + 18.0f * ph, 0.3f + 0.9f * ph,
                {0.6f, 0.6f, 0.62f}, 0.45f * busy * (1.0f - ph));
    }
}

inline void drawHeadframe(Host& h, const Pithead& p, float t) {
    if (p.appear < 0.02f) return;
    const float x = p.x, top = 6.0f * p.appear, z = 0.1f;
    stick(h, x - 1.3f, 0.1f, x, top, z, kWood, 0.16f);
    stick(h, x + 1.3f, 0.1f, x, top, z, kWood, 0.16f);
    stick(h, x - 1.3f, 0.1f, x, top, kBack - 0.6f, kWoodDark, 0.16f);
    stick(h, x + 1.3f, 0.1f, x, top, kBack - 0.6f, kWoodDark, 0.16f);
    for (int k = 1; k <= 2; ++k) {
        const float yy = top * k / 3.0f, half = 1.3f * (1.0f - static_cast<float>(k) / 3.0f);
        stick(h, x - half, yy, x + half, yy, z, kWood, 0.1f);
    }
    const float wy = top + 0.4f;
    drawSheave(h, x, wy, z, t * 1.6f * p.busy);
    // A pennant in the place's own colour.
    stick(h, x, wy + 0.7f, x, wy + 1.6f, z, kWoodDark, 0.05f);
    const float flap = 0.08f * std::sin(t * 3.0f + p.seed * 10.0f);
    h.add(STRATA_PRIMITIVE_CUBE, x + 0.32f, wy + 1.4f + flap, z, 0.6f, 0.34f, 0.04f, shirtOf(p.seed));
    drawWindingHouse(h, x, p.seed, p.busy, t);
}

// Planks across the collar, a cross of boards below it, rubble in the chamber.
inline void drawBoardedUp(Host& h, const Lead& l) {
    const float x = l.x;
    for (int k = 0; k < 3; ++k)
        h.add(STRATA_PRIMITIVE_CUBE, x, 0.4f + 0.4f * k, 0.55f, kShaftHalf * 2.6f, 0.22f, 0.08f, mul(kWood, 0.7f + 0.1f * k),
              l.offline);
    stick(h, x - kShaftHalf, -0.3f, x + kShaftHalf, -2.2f, 0.6f, mul(kWood, 0.8f), 0.18f);
    stick(h, x + kShaftHalf, -0.3f, x - kShaftHalf, -2.2f, 0.6f, mul(kWood, 0.8f), 0.18f);
    for (int k = 0; k < 6; ++k)
        h.add(STRATA_PRIMITIVE_SPHERE, x - 1.3f + k * 0.5f, l.face + 0.18f + 0.1f * hash1(k + l.seed), 0.2f, 0.5f,
              0.36f + 0.2f * hash1(k * 3.0f + l.seed), 0.5f, {0.3f, 0.27f, 0.25f});
}

// Collar, a little headgear unless the place's headframe stands over it, the
// session's colour tag, timber sets, a ladder and the chamber rails.
inline void drawShaft(Host& h, const Lead& l) {
    const float x = l.x, face = l.face;
    h.add(STRATA_PRIMITIVE_CUBE, x - kShaftHalf - 0.12f, 0.15f, 0.2f, 0.24f, 0.3f, 1.3f, kWood);
    h.add(STRATA_PRIMITIVE_CUBE, x + kShaftHalf + 0.12f, 0.15f, 0.2f, 0.24f, 0.3f, 1.3f, kWood);
    if (!l.underHeadframe) {
        stick(h, x - kShaftHalf - 0.1f, 0.3f, x - kShaftHalf - 0.1f, 1.7f, 0.0f, kWood, 0.12f);
        stick(h, x + kShaftHalf + 0.1f, 0.3f, x + kShaftHalf + 0.1f, 1.7f, 0.0f, kWood, 0.12f);
        stick(h, x - kShaftHalf - 0.25f, 1.7f, x + kShaftHalf + 0.25f, 1.7f, 0.0f, kWood, 0.14f);
    }
    h.add(STRATA_PRIMITIVE_CUBE, x, l.underHeadframe ? 0.45f : 1.86f, 0.55f, 0.4f, 0.2f, 0.2f, shirtOf(l.seed));
    for (float y = -1.2f; y > face + 0.6f; y -= 1.8f)
        h.add(STRATA_PRIMITIVE_CUBE, x, y, kBack + 0.08f, kShaftHalf * 2.0f, 0.14f, 0.12f, kWoodDark);
    const float lx = x - kShaftHalf + 0.18f;
    h.line(lx, 0.5f, 0.0f, lx, face, 0.0f, kLadder);
    h.line(lx + 0.32f, 0.5f, 0.0f, lx + 0.32f, face, 0.0f, kLadder);
    for (float y = 0.3f; y > face; y -= 0.4f) h.line(lx, y, 0.0f, lx + 0.32f, y, 0.0f, kLadder);
    h.line(x - kChamberHalf, face + 0.03f, 0.05f, x + kChamberHalf, face + 0.03f, 0.05f, kIron);
    h.line(x - kChamberHalf, face + 0.03f, 0.45f, x + kChamberHalf, face + 0.03f, 0.45f, kIron);
    if (l.offline > 0.02f) drawBoardedUp(h, l);
}

// Props every so often, rails along the floor.
inline void drawTunnel(Host& h, const Tunnel& t) {
    const float a = t.sx + t.side * kShaftHalf, b = tunnelEnd(t);
    for (float d = 0.9f; d < t.len - 0.3f; d += 1.5f) {
        const float px = t.sx + t.side * (kShaftHalf + d);
        h.add(STRATA_PRIMITIVE_CUBE, px, t.y + kTunnelHigh * 0.5f, kBack + 0.12f, 0.14f, kTunnelHigh, 0.14f, kWoodDark);
        h.add(STRATA_PRIMITIVE_CUBE, px, t.y + kTunnelHigh - 0.08f, 0.1f, 0.3f, 0.14f, 1.3f, kWood);
    }
    h.line(a, t.y + 0.03f, 0.05f, b, t.y + 0.03f, 0.05f, kIron);
    h.line(a, t.y + 0.03f, 0.45f, b, t.y + 0.03f, 0.45f, kIron);
}

// Why the session waits, on the cage: a padlock (permission) or a "?" (question).
inline void drawReason(Host& h, float cx, agentviz::WaitingReason reason, float level) {
    const Rgb c{1.0f, 0.92f, 0.85f};
    const float z = 0.75f;
    if (reason == agentviz::WaitingReason::Permission) {
        ring(h, cx, 1.02f, z, 0.16f, c, level, 14);
        h.add(STRATA_PRIMITIVE_CUBE, cx, 0.82f, z, 0.42f, 0.32f, 0.08f, {0.95f, 0.78f, 0.25f}, level);
        h.add(STRATA_PRIMITIVE_SPHERE, cx, 0.84f, z + 0.05f, 0.08f, 0.08f, 0.04f, {0.1f, 0.06f, 0.02f}, level);
    } else if (reason == agentviz::WaitingReason::Question) {
        const float y = 0.75f;  // a hook and a dot, 0.7 tall
        const float hook[6][2] = {{-0.15f, 0.52f}, {-0.1f, 0.65f}, {0.08f, 0.68f}, {0.16f, 0.55f}, {0.0f, 0.38f}, {0.0f, 0.25f}};
        for (int k = 0; k < 5; ++k)
            stick(h, cx + hook[k][0], y + hook[k][1], cx + hook[k + 1][0], y + hook[k + 1][1], z, c, 0.08f);
        h.add(STRATA_PRIMITIVE_SPHERE, cx, y + 0.08f, z, 0.11f, 0.11f, 0.06f, c, level);
    }
}

// A flare rises over the shaft and bursts, again and again.
inline void drawFlare(Host& h, float x, float level, float t, float pulse) {
    const float ph = frac(t * 0.45f);
    const float fy = 2.2f + 4.5f * std::sin(std::min(ph, 0.5f) * kPi);
    h.point(x, fy, 0.6f, 22.0f, 0.9f + 1.0f * pulse, kRed, level);
    if (ph <= 0.5f) return;
    const float age = ph - 0.5f;
    for (int k = 0; k < 12; ++k) {
        const float a = k * 2 * kPi / 12;
        h.point(x + std::cos(a) * age * 4.0f, fy + std::sin(a) * age * 4.0f - age * age * 3.0f, 0.6f, 6.0f, 0.15f,
                {1.0f, 0.35f, 0.2f}, level * (1.0f - age * 2.0f), 0);
    }
}

// The lift cage at the surface, rattling, a flare overhead and a pillar of
// red light standing over the shaft, seen from anywhere.
inline void drawCage(Host& h, const Lead& l, float t) {
    const float level = l.waiting, x = l.x;
    if (level < 0.02f) return;
    const float cx = x + 0.04f * std::sin(t * 30.0f) * level, y0 = 0.35f, y1 = 1.5f, hw = 0.55f;
    for (int k = 0; k <= 4; ++k) {
        const float bx = cx - hw + k * hw * 0.5f;
        stick(h, bx, y0, bx, y1, 0.55f, kCageBar, 0.05f);
    }
    stick(h, cx - hw, y0, cx + hw, y0, 0.55f, kCageBar, 0.08f);
    stick(h, cx - hw, y1, cx + hw, y1, 0.55f, kCageBar, 0.08f);
    drawReason(h, cx, l.reason, level);
    const float pulse = 0.5f + 0.5f * std::sin(t * 6.0f);
    drawFlare(h, x, level, t, pulse);
    h.point(x, 1.95f, 0.7f, 22.0f, 1.2f + 0.8f * pulse, kRed, level * (0.6f + 0.4f * pulse));
    for (int k = 0; k < 14; ++k) {
        const float yy = 2.4f + k * 0.8f, w = 1.0f - k / 14.0f;
        h.point(x, yy, 0.4f, 10.0f, 0.9f * w + 0.3f, kRed, level * w * (0.35f + 0.35f * pulse));
    }
    h.add(STRATA_PRIMITIVE_SPHERE, x, 1.95f, 0.3f, 0.3f, 0.3f, 0.3f, mix({0.4f, 0.05f, 0.03f}, {1.0f, 0.2f, 0.1f}, pulse));
    h.light(x, 2.4f, 1.2f, 9.0f, kRed, (1.5f + 2.5f * pulse) * level, 20);
}

// A red signal running up the shaft from the miner who rang to the cage.
inline void drawSignal(Host& h, const Lead& l, float t) {
    if (l.waiting <= 0.02f) return;
    for (int k = 0; k < 5; ++k) {
        const float ph = frac(t * 0.9f + k / 5.0f);
        h.point(l.x + l.facing * 0.3f, l.face + 1.8f + ph * (-l.face - 0.4f), 0.4f, 8.0f, 0.22f, kRed,
                l.waiting * (1.0f - ph * 0.5f));
    }
}

// Stalled: dust churning round the face, pebbles dropping, an amber warning
// lamp blinking over it. `s` scales it down for a subagent's tunnel.
inline void drawStalled(Host& h, float x, float y, float level, float t, float seed, float s = 1.0f) {
    if (level < 0.02f) return;
    for (int k = 0; k < 18; ++k) {
        const float a = t * (0.6f + 0.3f * hash1(k + seed)) + k * 2.4f;
        const float r = (0.4f + 1.2f * hash1(k * 1.7f + seed)) * s;
        h.point(x + std::cos(a) * r, y + (1.0f + std::sin(a * 0.7f) * 0.8f) * s, 0.3f, 22.0f * s, 0.9f * s,
                {0.62f, 0.56f, 0.46f}, 0.5f * level);
    }
    for (int k = 0; k < 4; ++k) {
        const float ph = frac(t * 0.8f + k / 4.0f);
        h.add(STRATA_PRIMITIVE_CUBE, x + (-1.0f + k * 0.6f) * s, y + (2.2f - ph * 2.2f) * s, 0.2f, 0.1f, 0.1f, 0.1f,
              {0.35f, 0.3f, 0.26f});
    }
    const float flick = 0.6f + 0.4f * std::sin(t * 9.0f) * std::sin(t * 3.7f);
    h.light(x, y + 1.6f * s, 0.8f, 7.0f * s, {1.0f, 0.55f, 0.1f}, 2.6f * flick * level, 12);
    h.add(STRATA_PRIMITIVE_SPHERE, x, y + 2.05f * s, kBack + 0.3f, 0.22f * s, 0.22f * s, 0.22f * s,
          mix({0.3f, 0.15f, 0.02f}, {1.0f, 0.6f, 0.1f}, flick));
    h.point(x, y + 2.05f * s, 0.5f, 18.0f * s, 0.8f * s, kAmber, 0.85f * flick * level);
}

}  // namespace mine
