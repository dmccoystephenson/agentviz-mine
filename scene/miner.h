// miner.h: a miner (helmet, lamp, shirt in the session's colour) and what he
// is doing: one function per action, plus waiting, idle and resting poses,
// the per-tool-call chip burst and the selection brackets.
#pragma once

#include "workings.h"

namespace mine {

using agentviz::Action;

struct MinerPose {
    float x, y, z, facing, scale;
    Rgb shirt;
    bool sitting;
};

// What a miner is doing. wallX is the working face's x; ropeX the shaft's x
// and ropeTurnY the roof height where a bell rope turns towards it.
struct Work {
    Action action;
    float t, seed, wallX, ropeX, ropeTurnY, pulse;
    bool resting, idle, waiting;
};

// How the miner's light is ranked and how far it reaches.
struct Light {
    float level, alpha;
    Rgb tint;
    int priority;
    float range;
};

// Hand position, for the tools.
struct Hand {
    float x, y;
};

// Draws the body; returns the helmet lamp's position in lampOut.
inline void drawMiner(Host& h, const MinerPose& p, float alpha, float* lampOut) {
    const float s = p.scale, f = p.facing, x = p.x, y = p.y, z = p.z;
    const float drop = p.sitting ? 0.32f * s : 0.0f;
    const Rgb shirt = mul(p.shirt, 0.35f + 0.65f * alpha);
    if (p.sitting) {
        h.add(STRATA_PRIMITIVE_CUBE, x + f * 0.22f * s, y + 0.12f * s, z - 0.08f * s, 0.55f * s, 0.16f * s, 0.14f * s, kTrousers);
        h.add(STRATA_PRIMITIVE_CUBE, x + f * 0.22f * s, y + 0.12f * s, z + 0.12f * s, 0.55f * s, 0.16f * s, 0.14f * s, kTrousers);
    } else {
        h.add(STRATA_PRIMITIVE_CUBE, x, y + 0.26f * s, z - 0.09f * s, 0.15f * s, 0.52f * s, 0.15f * s, kTrousers);
        h.add(STRATA_PRIMITIVE_CUBE, x, y + 0.26f * s, z + 0.09f * s, 0.15f * s, 0.52f * s, 0.15f * s, kTrousers);
    }
    h.add(STRATA_PRIMITIVE_CAPSULE, x, y + 0.82f * s - drop, z, 0.44f * s, 0.34f * s, 0.36f * s, shirt);
    const float hy = y + 1.3f * s - drop;
    h.add(STRATA_PRIMITIVE_SPHERE, x + f * 0.03f * s, hy, z, 0.32f * s, 0.32f * s, 0.32f * s, kSkin);
    h.add(STRATA_PRIMITIVE_SPHERE, x, hy + 0.1f * s, z, 0.36f * s, 0.22f * s, 0.36f * s, kHelmet);
    h.add(STRATA_PRIMITIVE_CYLINDER, x, hy + 0.06f * s, z, 0.44f * s, 0.025f * s, 0.44f * s, kHelmet);
    lampOut[0] = x + f * 0.19f * s;
    lampOut[1] = hy + 0.12f * s;
    lampOut[2] = z;
}

inline void lampGlow(Host& h, const float* lp, float level, Rgb tint, float scale) {
    const Rgb c = mix(kLampWarm, tint, 0.25f);
    h.add(STRATA_PRIMITIVE_SPHERE, lp[0], lp[1], lp[2], 0.1f * scale, 0.1f * scale, 0.1f * scale,
          mix({0.25f, 0.22f, 0.18f}, {1.0f, 0.95f, 0.75f}, level));
    if (level > 0.02f) h.point(lp[0], lp[1], lp[2] + 0.12f, 6.0f, 0.55f * scale * (0.5f + 0.5f * level), c, 0.85f * level);
}

inline void sparks(Host& h, float wx, float wy, float z, float f, float age, float seed, Rgb c) {
    if (age < 0.0f || age > 0.45f) return;
    for (int k = 0; k < 10; ++k) {
        const float vx = -f * (0.8f + 2.2f * hash1(seed * 13.0f + k)), vy = 0.6f + 2.4f * hash1(seed * 7.0f + k * 3.3f);
        h.point(wx + vx * age, wy + vy * age - 4.9f * age * age, z + 0.2f, 3.5f, 0.06f, c, 1.0f - age / 0.45f, 0);
    }
}

// The pick: a handle from the hand at theta degrees above the horizontal
// (towards the facing side), with its head across the end.
inline void pickaxe(Host& h, Hand hand, float z, float f, float theta, float s) {
    const float r = theta * kPi / 180.0f, dx = f * std::cos(r), dy = std::sin(r);
    const float ex = hand.x + dx * 0.75f * s, ey = hand.y + dy * 0.75f * s;
    stick(h, hand.x, hand.y, ex, ey, z + 0.25f * s, kWood, 0.06f * s);
    const float nx = -dy, ny = dx;
    stick(h, ex - nx * 0.28f * s, ey - ny * 0.28f * s, ex + nx * 0.3f * s, ey + ny * 0.3f * s, z + 0.25f * s, kSteel, 0.08f * s);
}

// edit: the pick swings and sparks fly off the face at each strike.
inline void drawEdit(Host& h, const MinerPose& p, const Work& w, Hand hand, int priority) {
    const float s = p.scale, f = p.facing;
    const float ph = frac(w.t * 1.15f + w.seed * 3.0f);
    float theta;
    if (ph < 0.62f) {
        const float u = ph / 0.62f;
        theta = -15.0f + 130.0f * (1.0f - (1.0f - u) * (1.0f - u));
    } else {
        const float u = (ph - 0.62f) / 0.38f;
        theta = 115.0f - 130.0f * u * u;
    }
    pickaxe(h, hand, p.z, f, theta, s);
    const float age = frac(ph - 0.995f) / 1.15f;
    sparks(h, w.wallX - f * 0.05f, p.y + 0.75f * s, p.z, f, age, w.seed + std::floor(w.t * 1.15f), {1.0f, 0.8f, 0.3f});
    if (age < 0.12f) h.light(w.wallX - f * 0.2f, p.y + 0.8f * s, 0.5f, 3.0f, {1.0f, 0.7f, 0.3f}, 2.0f, priority);
}

inline void oreCart(Host& h, float cx, float y, float z, float s) {
    h.add(STRATA_PRIMITIVE_CUBE, cx, y + 0.32f * s, z + 0.15f, 0.62f * s, 0.34f * s, 0.46f * s, {0.42f, 0.22f, 0.12f});
    h.add(STRATA_PRIMITIVE_SPHERE, cx - 0.2f * s, y + 0.11f * s, z + 0.4f, 0.2f * s, 0.2f * s, 0.06f * s, kIron);
    h.add(STRATA_PRIMITIVE_SPHERE, cx + 0.2f * s, y + 0.11f * s, z + 0.4f, 0.2f * s, 0.2f * s, 0.06f * s, kIron);
    for (int k = 0; k < 3; ++k)
        h.add(STRATA_PRIMITIVE_SPHERE, cx + (k - 1) * 0.17f * s, y + 0.52f * s, z + 0.15f, 0.2f * s, 0.16f * s, 0.2f * s,
              k == 1 ? Rgb{0.75f, 0.6f, 0.2f} : Rgb{0.2f, 0.2f, 0.22f});
}

// run: a rock drill at the face throwing dust, an ore cart rolling behind.
inline void drawRun(Host& h, const MinerPose& p, const Work& w, Hand hand) {
    const float s = p.scale, f = p.facing, z = p.z, t = w.t;
    const float shake = 0.025f * s * std::sin(t * 70.0f);
    const float bx = hand.x, by = hand.y - 0.1f * s + shake, tipX = w.wallX - f * 0.05f;
    stick(h, bx, by, bx + f * 0.45f * s, by, z + 0.25f * s, {0.85f, 0.62f, 0.12f}, 0.2f * s);
    stick(h, bx + f * 0.45f * s, by, tipX, by, z + 0.25f * s, {0.6f, 0.62f, 0.66f}, 0.05f * s);
    for (int k = 0; k < 8; ++k) {
        const float ph = frac(t * 2.4f + k / 8.0f);
        h.point(tipX - f * ph * 0.9f * s, by + (hash1(k * 5.1f) - 0.3f) * ph * 0.9f * s, z + 0.3f, 4.0f + 8.0f * ph,
                0.12f * ph, {0.62f, 0.55f, 0.45f}, 0.7f * (1.0f - ph));
    }
    const float roll = 0.5f + 0.5f * std::sin(t * 1.3f + w.seed * 6.0f);
    oreCart(h, p.x - f * (0.75f + 0.9f * roll) * s, p.y, z, s);
}

// read: the lamp held up to a glittering vein of ore.
inline void drawRead(Host& h, const MinerPose& p, const Work& w, const Light& light) {
    const float s = p.scale, f = p.facing, z = p.z;
    const float lx = p.x + f * 0.6f * s, ly = p.y + 1.15f * s;
    stick(h, p.x + f * 0.1f * s, p.y + 0.95f * s, lx, ly, z + 0.2f * s, p.shirt, 0.09f * s);
    h.add(STRATA_PRIMITIVE_SPHERE, lx, ly - 0.1f * s, z + 0.25f * s, 0.16f * s, 0.22f * s, 0.16f * s, {1.0f, 0.92f, 0.6f});
    h.point(lx, ly - 0.1f * s, z + 0.4f, 8.0f, 0.6f * s, kLampWarm, 0.9f);
    h.light(lx, ly, 0.8f, light.range * 0.8f, kLampWarm, 1.6f, light.priority);
    for (int k = 0; k < 7; ++k) {
        const float vy = p.y + (0.3f + k * 0.24f) * s, vz = 0.1f + 0.1f * k;
        const Rgb g = (k % 2) ? kGold : kCyan;
        h.add(STRATA_PRIMITIVE_SPHERE, w.wallX, vy, vz, 0.16f * s, 0.12f * s, 0.12f * s, g);
        const float tw = 0.5f + 0.5f * std::sin(w.t * 5.0f + k * 1.9f + w.seed * 9.0f);
        h.point(w.wallX - f * 0.05f, vy, vz + 0.1f, 3.0f + 5.0f * tw, 0.0f, g, tw, 0);
    }
}

// web: pulling the bell rope (up to the roof, along to the shaft, up the
// shaft), blue signals racing up it.
inline void drawWeb(Host& h, const MinerPose& p, const Work& w, Hand hand) {
    const float s = p.scale, z = p.z;
    const float rx = hand.x, ry = hand.y + 0.25f * s + 0.08f * s * std::sin(w.t * 5.0f);
    stick(h, p.x, p.y + 1.0f * s, rx, ry, z + 0.2f * s, p.shirt, 0.09f * s);
    const float pts[4][2] = {{rx, ry}, {rx, w.ropeTurnY}, {w.ropeX, w.ropeTurnY}, {w.ropeX, 1.55f}};
    float total = 0.0f, segLen[3];
    for (int k = 0; k < 3; ++k) {
        segLen[k] = std::fabs(pts[k + 1][0] - pts[k][0]) + std::fabs(pts[k + 1][1] - pts[k][1]);
        total += segLen[k];
        h.line(pts[k][0], pts[k][1], z + 0.2f, pts[k + 1][0], pts[k + 1][1], z + 0.2f, {0.75f, 0.65f, 0.45f});
    }
    for (int n = 0; n < 4; ++n) {
        float d = frac(w.t * 0.7f + n / 4.0f) * total;
        int k = 0;
        while (k < 2 && d > segLen[k]) d -= segLen[k++];
        const float u = segLen[k] > 0.0f ? std::min(d / segLen[k], 1.0f) : 0.0f;
        h.point(pts[k][0] + (pts[k + 1][0] - pts[k][0]) * u, pts[k][1] + (pts[k + 1][1] - pts[k][1]) * u, z + 0.3f, 9.0f,
                0.18f, {0.55f, 0.85f, 1.0f}, 0.95f);
    }
}

// delegate: pointing as new tunnel mouths open in the shaft walls.
inline void drawDelegate(Host& h, const MinerPose& p, const Work& w) {
    const float s = p.scale, f = p.facing;
    stick(h, p.x, p.y + 1.0f * s, p.x + f * 0.55f * s, p.y + 1.25f * s, p.z + 0.2f * s, p.shirt, 0.09f * s);
    for (int k = 0; k < 3; ++k) {
        const float ph = frac(w.t * 0.45f + k / 3.0f);
        const float side = (k % 2) ? -1.0f : 1.0f;
        const float mx = w.ropeX + side * (kShaftHalf - 0.02f), my = p.y + (0.7f + 0.55f * k) * s + 2.0f;
        ring(h, mx, my, kBack + 0.1f, 0.12f + 0.45f * ph, {0.95f, 0.8f, 0.45f}, 1.0f - ph, 14);
        h.point(mx, my, kBack + 0.2f, 10.0f * ph + 2.0f, 0.4f * ph, {0.5f, 0.45f, 0.38f}, 0.6f * (1.0f - ph));
    }
}

// think: sitting by the upturned pick, thoughts rising.
inline void drawThink(Host& h, const MinerPose& p, const Work& w) {
    const float s = p.scale, f = p.facing, z = p.z;
    const float px = p.x + f * 0.42f * s;
    stick(h, px, p.y, px, p.y + 0.95f * s, z + 0.2f * s, kWood, 0.06f * s);
    stick(h, px - 0.3f * s, p.y + 0.95f * s, px + 0.3f * s, p.y + 0.98f * s, z + 0.2f * s, kSteel, 0.08f * s);
    for (int k = 0; k < 3; ++k) {
        const float ph = frac(w.t * 0.3f + k / 3.0f);
        h.point(p.x - f * 0.1f * s + 0.2f * std::sin(ph * 6.0f), p.y + (1.5f + ph * 1.3f) * s, z + 0.3f, 4.0f + 10.0f * ph,
                0.08f + 0.22f * ph, {0.95f, 0.95f, 1.0f}, 0.75f * (1.0f - ph * ph));
    }
}

// other: shovelling spoil over the shoulder.
inline void drawOther(Host& h, const MinerPose& p, const Work& w, Hand hand) {
    const float s = p.scale, f = p.facing, z = p.z;
    const float ph = frac(w.t * 0.9f + w.seed * 2.0f);
    const float r = ((ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f) * 70.0f - 25.0f) * kPi / 180.0f;
    const float ex = hand.x + f * std::cos(r) * 0.7f * s, ey = hand.y + std::sin(r) * 0.7f * s;
    stick(h, hand.x, hand.y, ex, ey, z + 0.25f * s, kWood, 0.05f * s);
    h.add(STRATA_PRIMITIVE_CUBE, ex, ey, z + 0.25f * s, 0.2f * s, 0.06f * s, 0.2f * s, kIron);
    if (ph <= 0.5f) return;
    const float age = (ph - 0.5f) * 1.1f;
    for (int k = 0; k < 6; ++k) {
        const float vx = -f * (0.8f + hash1(k * 2.3f)), vy = 1.6f + hash1(k * 4.1f);
        h.point(ex + vx * age, ey + vy * age - 4.9f * age * age, z + 0.3f, 5.0f, 0.1f, {0.4f, 0.28f, 0.16f}, 1.0f, 0);
    }
}

// waiting: waving both arms for the cage.
inline void drawWave(Host& h, const MinerPose& p, float t) {
    const float s = p.scale, z = p.z, wave = std::sin(t * 7.0f) * 0.35f;
    stick(h, p.x - 0.15f * s, p.y + 1.0f * s, p.x - (0.35f + wave) * s, p.y + 1.75f * s, z + 0.2f * s, p.shirt, 0.09f * s);
    stick(h, p.x + 0.15f * s, p.y + 1.0f * s, p.x + (0.35f - wave) * s, p.y + 1.75f * s, z + 0.2f * s, p.shirt, 0.09f * s);
}

// idle: dozing, slow breath drifting up.
inline void drawDoze(Host& h, const MinerPose& p, const Work& w) {
    const float s = p.scale, f = p.facing;
    for (int k = 0; k < 3; ++k) {
        const float ph = frac(w.t * 0.25f + k / 3.0f + w.seed);
        h.point(p.x + f * (0.3f + ph * 0.5f) * s, p.y + (1.4f + ph * 1.0f) * s, p.z + 0.2f, 4.0f + 6.0f * ph, 0.0f,
                {0.7f, 0.8f, 1.0f}, 0.6f * (1.0f - ph));
    }
}

// A resting subagent: its lamp hung on a prop, still lighting the tunnel.
inline void drawHungLamp(Host& h, const MinerPose& p, const Work& w, const Light& light) {
    const float s = p.scale, px = w.wallX - p.facing * 0.6f * s, py = p.y + 1.45f * s;
    stick(h, px, p.y, px, py + 0.2f * s, kBack + 0.2f, kWood, 0.1f * s);
    const float hung[3] = {px, py, kBack + 0.45f};
    lampGlow(h, hung, 0.9f, light.tint, s);
    h.light(px, py, 0.6f, light.range, mix(kLampWarm, light.tint, 0.2f), 1.1f, light.priority);
}

// A new tool call (the wire's pulse, 1 fading to 0): chips of rock burst off
// the face with a white flash.
inline void drawToolPulse(Host& h, const MinerPose& p, const Work& w) {
    if (w.pulse < 0.02f) return;
    const float s = p.scale, f = p.facing, age = (1.0f - w.pulse) * 0.6f;
    const float fx = w.wallX - f * 0.1f, fy = p.y + 1.0f * s;
    h.point(fx, fy, p.z + 0.3f, 26.0f, 0.5f * s, {1.0f, 0.97f, 0.85f}, w.pulse * w.pulse);
    for (int k = 0; k < 8; ++k) {
        const float vx = -f * (1.0f + 2.0f * hash1(k * 3.7f + w.seed)), vy = 0.5f + 2.5f * hash1(k * 1.9f + w.seed);
        h.add(STRATA_PRIMITIVE_CUBE, fx + vx * age, fy + vy * age - 4.9f * age * age, p.z + 0.25f, 0.09f * s, 0.09f * s,
              0.09f * s, {0.45f, 0.38f, 0.30f}, w.pulse);
    }
}

inline void drawAction(Host& h, const MinerPose& p, const Work& w, Hand hand, const Light& light) {
    switch (w.action) {
        case Action::Edit: drawEdit(h, p, w, hand, light.priority); break;
        case Action::Run: drawRun(h, p, w, hand); break;
        case Action::Read: drawRead(h, p, w, light); break;
        case Action::Web: drawWeb(h, p, w, hand); break;
        case Action::Delegate: drawDelegate(h, p, w); break;
        case Action::Think: drawThink(h, p, w); break;
        default: drawOther(h, p, w, hand); break;
    }
}

// The miner, posed for his state and action, with his helmet lamp lit.
inline void drawAt(Host& h, const MinerPose& base, const Work& w, const Light& light) {
    MinerPose p = base;
    const float s = p.scale, f = p.facing;
    p.sitting = w.resting || w.idle || w.action == Action::Think;
    if (w.action == Action::Read && !p.sitting) p.x += f * 0.08f * s;  // leaning into the vein
    float lamp[3];
    drawMiner(h, p, light.alpha, lamp);
    const Hand hand{p.x + f * 0.22f * s, p.y + (p.sitting ? 0.62f : 0.95f) * s};
    float lampLevel = light.level;
    if (w.waiting) drawWave(h, p, w.t);
    else if (w.idle) drawDoze(h, p, w);
    else if (w.resting) { drawHungLamp(h, p, w, light); lampLevel = 0.15f; }
    else drawAction(h, p, w, hand, light);
    drawToolPulse(h, p, w);
    lampGlow(h, lamp, lampLevel, light.tint, s);
    if (lampLevel > 0.05f)
        h.light(lamp[0] + f * 0.3f, lamp[1], 0.7f, light.range * (0.45f + 0.55f * lampLevel), mix(kLampWarm, light.tint, 0.25f),
                0.4f + 1.4f * lampLevel, light.priority);
}

// Yellow corner brackets and a bobbing marker round the selected miner.
inline void selectionMark(Host& h, float x, float y, float s, float t) {
    const Rgb c{1.0f, 0.95f, 0.55f};
    const float x0 = x - 0.7f * s, x1 = x + 0.7f * s, y0 = y - 0.05f, y1 = y + 1.9f * s, z = 0.8f, k = 0.3f * s;
    const float corners[4][2] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
    for (const auto& cn : corners) {
        const float sx = cn[0] == x0 ? 1.0f : -1.0f, sy = cn[1] == y0 ? 1.0f : -1.0f;
        stick(h, cn[0], cn[1], cn[0] + sx * k, cn[1], z, c, 0.05f);
        stick(h, cn[0], cn[1], cn[0], cn[1] + sy * k, z, c, 0.05f);
    }
    h.point(x, y1 + 0.35f + 0.12f * std::sin(t * 4.0f), z, 12.0f, 0.3f * s, c, 0.95f);
}

}  // namespace mine
