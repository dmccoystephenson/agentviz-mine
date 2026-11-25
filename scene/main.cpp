// agentviz-mine: Claude Code sessions as a mine, seen in cross-section.
//
// The page (web/mine.js) owns the data and the layout: it reads /api/agents,
// places every pithead (place), shaft (session) and tunnel (subagent), eases
// between snapshots and hands this file flat float rows every frame. This
// file only draws: the banded earth cut away round the workings, timber,
// rails, the miners and what each one is doing.
#define STRATA_KIT_MAIN
#include "strata_kit.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

using strata_kit::Host;
using strata_kit::Rgb;

constexpr int kPlaceStride = 6;     // x, halfWidth, seed, waiting, busy, appear
constexpr int kSessionStride = 14;  // see mn_set_sessions
constexpr int kFollowerStride = 12; // see mn_set_followers
constexpr float kPi = 3.14159265f;

// The cut face of the earth runs from z = kBack (the back of every cavity) to
// z = kFront (the cross-section's face, nearest the camera).
constexpr float kBack = -0.55f, kFront = 0.9f;
constexpr float kShaftHalf = 0.8f, kChamberHalf = 1.9f, kChamberHigh = 2.3f, kTunnelHigh = 1.8f;

std::vector<float> places, sessions, followers;
float worldMin = -10.0f, worldMax = 10.0f, worldBottom = -12.0f;

// States and actions, as the page codes them.
enum { ST_OFFLINE = 0, ST_DIM, ST_IDLE, ST_BUSY, ST_WAITING };
enum { AC_NONE = 0, AC_EDIT, AC_RUN, AC_READ, AC_WEB, AC_DELEGATE, AC_THINK, AC_OTHER };

const Rgb kShirts[8] = {{0.85f, 0.30f, 0.22f}, {0.12f, 0.60f, 0.58f}, {0.85f, 0.65f, 0.12f}, {0.50f, 0.30f, 0.78f},
                        {0.45f, 0.72f, 0.20f}, {0.20f, 0.48f, 0.85f}, {0.80f, 0.25f, 0.60f}, {0.92f, 0.48f, 0.14f}};
const Rgb kWood{0.36f, 0.22f, 0.10f}, kWoodDark{0.22f, 0.13f, 0.06f}, kIron{0.30f, 0.31f, 0.34f};
const Rgb kSkin{0.78f, 0.56f, 0.42f}, kTrousers{0.16f, 0.18f, 0.24f}, kHelmet{0.88f, 0.70f, 0.14f};
const Rgb kLampWarm{1.0f, 0.86f, 0.55f}, kRed{1.0f, 0.12f, 0.08f};

// Strata bands, top to bottom, cycled with depth.
struct Band { float thick; Rgb c; };
const Band kBands[] = {
    {1.1f, {0.24f, 0.15f, 0.08f}},  // topsoil
    {1.6f, {0.46f, 0.25f, 0.13f}},  // clay
    {2.2f, {0.62f, 0.46f, 0.26f}},  // sandstone
    {1.4f, {0.30f, 0.29f, 0.31f}},  // shale
    {0.7f, {0.05f, 0.05f, 0.06f}},  // coal seam
    {2.4f, {0.60f, 0.57f, 0.50f}},  // limestone
    {1.8f, {0.50f, 0.20f, 0.12f}},  // red sandstone
    {0.6f, {0.06f, 0.06f, 0.07f}},  // coal seam
    {2.6f, {0.42f, 0.40f, 0.42f}},  // granite
    {1.9f, {0.20f, 0.23f, 0.28f}},  // slate
};
constexpr int kBandCount = sizeof(kBands) / sizeof(kBands[0]);

float hash1(float n) {
    const float s = std::sin(n * 127.1f + 311.7f) * 43758.5453f;
    return s - std::floor(s);
}
float frac(float v) { return v - std::floor(v); }
Rgb mul(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }
Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
Rgb shirtOf(float seed) { return kShirts[std::clamp(static_cast<int>(seed * 8.0f), 0, 7)]; }

// Depth (positive, below the surface) where band boundary k lies at x.
float boundaryDepth(int k, float x) {
    float d = 0.0f;
    for (int i = 0; i < k; ++i) d += kBands[i % kBandCount].thick;
    if (k == 0) return 0.0f;
    return d + 0.45f * std::sin(x * 0.11f + k * 1.7f) + 0.22f * std::sin(x * 0.31f + k * 2.9f);
}

void stick(Host& h, float x0, float y0, float x1, float y1, float z, Rgb c, float width) {
#if STRATA_ABI_MINOR >= 13
    h.wideLine(x0, y0, z, x1, y1, z, c, 2.0f, width);
#else
    h.line(x0, y0, z, x1, y1, z, c);
#endif
}

void ring(Host& h, float x, float y, float z, float r, Rgb c, float a, int n = 20) {
    for (int k = 0; k < n; ++k) {
        const float a0 = k * 2 * kPi / n, a1 = (k + 1) * 2 * kPi / n;
        h.line(x + std::cos(a0) * r, y + std::sin(a0) * r, z, x + std::cos(a1) * r, y + std::sin(a1) * r, z, c, a);
    }
}

// ------------------------------------------------------------------ cavities

struct Cavity { float x0, x1, y0, y1; };
std::vector<Cavity> cavities;

void collectCavities() {
    cavities.clear();
    for (size_t i = 0; i + kSessionStride <= sessions.size(); i += kSessionStride) {
        const float* s = &sessions[i];
        const float x = s[0], face = s[1];
        cavities.push_back({x - kShaftHalf, x + kShaftHalf, face, 0.6f});
        cavities.push_back({x - kChamberHalf, x + kChamberHalf, face, face + kChamberHigh});
    }
    for (size_t i = 0; i + kFollowerStride <= followers.size(); i += kFollowerStride) {
        const float* f = &followers[i];
        const float sx = f[0], y = f[1], side = f[2], len = f[3];
        const float a = sx + side * (kShaftHalf - 0.05f), b = sx + side * (kShaftHalf + len);
        cavities.push_back({std::min(a, b), std::max(a, b), y, y + kTunnelHigh});
    }
}

// The cut-away earth: columns of band-coloured blocks with the cavities left out.
void drawEarth(Host& h) {
    const float left = worldMin - 10.0f, right = worldMax + 10.0f, bottom = worldBottom - 3.0f;
    std::vector<float> xs;
    for (float x = left; x < right; x += 1.2f) xs.push_back(x);
    xs.push_back(right);
    for (const Cavity& c : cavities) {
        xs.push_back(c.x0);
        xs.push_back(c.x1);
    }
    std::sort(xs.begin(), xs.end());
    std::vector<std::pair<float, float>> holes;
    for (size_t i = 0; i + 1 < xs.size(); ++i) {
        const float a = xs[i], b = xs[i + 1];
        if (b - a < 0.02f) continue;
        const float cx = 0.5f * (a + b);
        holes.clear();
        for (const Cavity& c : cavities)
            if (cx > c.x0 && cx < c.x1) holes.push_back({c.y0, c.y1});
        std::sort(holes.begin(), holes.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        // Walk down from the surface, emitting rock between holes, split by band.
        float top = 0.0f;
        bool grass = true;
        auto rock = [&](float hi, float lo) {  // y values, hi > lo
            if (hi - lo < 0.01f) return;
            for (int k = 0; k < 40; ++k) {
                const float d0 = boundaryDepth(k, cx), d1 = boundaryDepth(k + 1, cx);
                const float y0 = std::min(hi, -d0), y1 = std::max(lo, -d1);
                if (y0 - y1 > 0.005f) {
                    const float depth = std::clamp(-0.5f * (y0 + y1) / 24.0f, 0.0f, 1.0f);
                    const float jitter = 0.92f + 0.16f * hash1(cx * 3.1f + k * 7.7f);
                    const Rgb c = mul(kBands[k % kBandCount].c, (1.0f - 0.45f * depth) * jitter);
                    h.add(STRATA_PRIMITIVE_CUBE, cx, 0.5f * (y0 + y1), 0.5f * (kBack + kFront), b - a, y0 - y1,
                          kFront - kBack, c);
                }
                if (-d1 < lo) break;
            }
        };
        // Merge overlapping holes as we go.
        for (const auto& hole : holes) {
            if (hole.second >= top) grass = false;
            if (hole.second < top) rock(top, hole.second);
            top = std::min(top, hole.first);
        }
        rock(top, bottom);
        if (grass)
            h.add(STRATA_PRIMITIVE_CUBE, cx, 0.07f, 0.5f * (kBack + kFront), b - a, 0.16f, kFront - kBack,
                  {0.22f, 0.36f, 0.12f});
        // A few glints of ore in the cut face.
        if (hash1(cx * 1.37f) > 0.7f) {
            const float gy = -2.0f - hash1(cx * 9.1f) * (-bottom - 3.0f);
            bool inHole = false;
            for (const auto& hole : holes) inHole = inHole || (gy > hole.first - 0.3f && gy < hole.second + 0.3f);
            if (!inHole) {
                const Rgb g = hash1(cx * 4.3f) > 0.5f ? Rgb{0.9f, 0.72f, 0.2f} : Rgb{0.3f, 0.75f, 0.8f};
                h.add(STRATA_PRIMITIVE_SPHERE, cx, gy, kFront, 0.16f, 0.12f, 0.08f, g);
            }
        }
    }
    // The cavities' back walls, darker than the face.
    for (const Cavity& c : cavities)
        h.add(STRATA_PRIMITIVE_CUBE, 0.5f * (c.x0 + c.x1), 0.5f * (c.y0 + std::min(c.y1, 0.0f)), kBack - 0.05f,
              c.x1 - c.x0, std::min(c.y1, 0.0f) - c.y0, 0.1f, {0.10f, 0.075f, 0.055f});
    // The land behind the cut, and bedrock under it.
    h.add(STRATA_PRIMITIVE_CUBE, 0.5f * (left + right), -0.2f, kBack - 40.0f, (right - left) + 120.0f, 0.4f, 80.0f,
          {0.20f, 0.33f, 0.11f});
    h.add(STRATA_PRIMITIVE_CUBE, 0.5f * (left + right), bottom - 30.0f, 0.5f * (kBack + kFront), right - left, 60.0f,
          kFront - kBack + 0.2f, {0.07f, 0.07f, 0.08f});
}

// ------------------------------------------------------------------ miners

struct MinerPose {
    float x, y, z, facing, scale;
    Rgb shirt;
    bool sitting;
};

// Draws a miner; returns the helmet lamp's position in out[3].
void drawMiner(Host& h, const MinerPose& p, float alpha, float* lampOut) {
    const float s = p.scale, f = p.facing, x = p.x, y = p.y, z = p.z;
    const float drop = p.sitting ? 0.32f * s : 0.0f;
    const Rgb shirt = mul(p.shirt, 0.35f + 0.65f * alpha);
    if (p.sitting) {
        h.add(STRATA_PRIMITIVE_CUBE, x + f * 0.22f * s, y + 0.12f * s, z - 0.08f * s, 0.55f * s, 0.16f * s, 0.14f * s,
              kTrousers);
        h.add(STRATA_PRIMITIVE_CUBE, x + f * 0.22f * s, y + 0.12f * s, z + 0.12f * s, 0.55f * s, 0.16f * s, 0.14f * s,
              kTrousers);
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

void lampGlow(Host& h, const float* lp, float level, Rgb tint, float scale) {
    const Rgb c = mix(kLampWarm, tint, 0.25f);
    h.add(STRATA_PRIMITIVE_SPHERE, lp[0], lp[1], lp[2], 0.1f * scale, 0.1f * scale, 0.1f * scale,
          mix({0.25f, 0.22f, 0.18f}, {1.0f, 0.95f, 0.75f}, level));
    if (level > 0.02f) h.point(lp[0], lp[1], lp[2] + 0.12f, 6.0f, 0.55f * scale * (0.5f + 0.5f * level), c, 0.85f * level);
}

void sparks(Host& h, float wx, float wy, float z, float f, float age, float seed, Rgb c) {
    if (age < 0.0f || age > 0.45f) return;
    for (int k = 0; k < 10; ++k) {
        const float vx = -f * (0.8f + 2.2f * hash1(seed * 13.0f + k)), vy = 0.6f + 2.4f * hash1(seed * 7.0f + k * 3.3f);
        const float px = wx + vx * age, py = wy + vy * age - 4.9f * age * age;
        h.point(px, py, z + 0.2f, 3.5f, 0.06f, c, 1.0f - age / 0.45f, 0);
    }
}

// The pick: a handle from the hand at angle theta (degrees from the
// horizontal, towards the facing side) with its head across the end.
void pick(Host& h, float hx, float hy, float z, float f, float theta, float s) {
    const float r = theta * kPi / 180.0f, dx = f * std::cos(r), dy = std::sin(r);
    const float ex = hx + dx * 0.75f * s, ey = hy + dy * 0.75f * s;
    stick(h, hx, hy, ex, ey, z + 0.25f * s, kWood, 0.06f * s);
    const float nx = -dy, ny = dx;
    stick(h, ex - nx * 0.28f * s, ey - ny * 0.28f * s, ex + nx * 0.3f * s, ey + ny * 0.3f * s, z + 0.25f * s,
          {0.55f, 0.56f, 0.6f}, 0.08f * s);
}

// What a miner is doing. (wx) is the working face's x, (top) the y where a
// bell rope tops out (the surface collar), (ropeX) the shaft's x.
struct Work {
    int action;
    float t, seed, wallX, ropeX, ropeTurnY;
    bool resting, idle, waiting;
};

void drawAt(Host& h, const MinerPose& base, const Work& w, float level, float alpha, Rgb tint, int lightPriority,
            float lightRange) {
    MinerPose p = base;
    const float s = p.scale, f = p.facing, t = w.t, z = p.z;
    const bool sit = w.resting || w.idle || w.action == AC_THINK;
    p.sitting = sit;
    float lamp[3];
    // A reading miner leans into the vein.
    if (w.action == AC_READ && !sit) p.x += f * 0.08f * s;
    drawMiner(h, p, alpha, lamp);
    const float handX = p.x + f * 0.22f * s, handY = p.y + (sit ? 0.62f : 0.95f) * s;
    float lampLevel = level;

    if (w.waiting) {
        // Waving both arms for the cage.
        const float wave = std::sin(t * 7.0f) * 0.35f;
        stick(h, p.x - 0.15f * s, p.y + 1.0f * s, p.x - (0.35f + wave) * s, p.y + 1.75f * s, z + 0.2f * s, p.shirt, 0.09f * s);
        stick(h, p.x + 0.15f * s, p.y + 1.0f * s, p.x + (0.35f - wave) * s, p.y + 1.75f * s, z + 0.2f * s, p.shirt, 0.09f * s);
    } else if (w.idle) {
        // Dozing: a few slow sparks of breath drifting up.
        for (int k = 0; k < 3; ++k) {
            const float ph = frac(t * 0.25f + k / 3.0f + w.seed);
            h.point(p.x + f * (0.3f + ph * 0.5f) * s, p.y + (1.4f + ph * 1.0f) * s, z + 0.2f, 4.0f + 6.0f * ph, 0.0f,
                    {0.7f, 0.8f, 1.0f}, 0.6f * (1.0f - ph));
        }
    } else if (w.resting) {
        // A resting subagent: its lamp hung on a prop, still lit.
        const float px = w.wallX - f * 0.6f * s, py = p.y + 1.45f * s;
        stick(h, px, p.y, px, py + 0.2f * s, kBack + 0.2f, kWood, 0.1f * s);
        const float hung[3] = {px, py, kBack + 0.45f};
        lampGlow(h, hung, 0.9f, tint, s);
        h.light(px, py, 0.6f, lightRange, mix(kLampWarm, tint, 0.2f), 1.1f, lightPriority);
        lampLevel = 0.15f;
    } else {
        switch (w.action) {
        case AC_EDIT: {  // Pickaxe swings, sparks off the face.
            const float ph = frac(t * 1.15f + w.seed * 3.0f);
            float theta;
            if (ph < 0.62f) {
                const float u = ph / 0.62f;
                theta = -15.0f + 130.0f * (1.0f - (1.0f - u) * (1.0f - u));
            } else {
                const float u = (ph - 0.62f) / 0.38f;
                theta = 115.0f - 130.0f * u * u;
            }
            pick(h, handX, handY, z, f, theta, s);
            const float age = frac(ph - 0.995f) / 1.15f;
            sparks(h, w.wallX - f * 0.05f, p.y + 0.75f * s, z, f, age, w.seed + std::floor(t * 1.15f), {1.0f, 0.8f, 0.3f});
            if (age < 0.12f) h.light(w.wallX - f * 0.2f, p.y + 0.8f * s, 0.5f, 3.0f, {1.0f, 0.7f, 0.3f}, 2.0f, lightPriority);
            break;
        }
        case AC_RUN: {  // A rock drill at the face and an ore cart rolling behind.
            const float shake = 0.025f * s * std::sin(t * 70.0f);
            const float bx = handX, by = handY - 0.1f * s + shake, tipX = w.wallX - f * 0.05f;
            stick(h, bx, by, bx + f * 0.45f * s, by, z + 0.25f * s, {0.85f, 0.62f, 0.12f}, 0.2f * s);
            stick(h, bx + f * 0.45f * s, by, tipX, by, z + 0.25f * s, {0.6f, 0.62f, 0.66f}, 0.05f * s);
            for (int k = 0; k < 8; ++k) {
                const float ph = frac(t * 2.4f + k / 8.0f);
                h.point(tipX - f * ph * 0.9f * s, by + (hash1(k * 5.1f) - 0.3f) * ph * 0.9f * s, z + 0.3f, 4.0f + 8.0f * ph,
                        0.12f * ph, {0.62f, 0.55f, 0.45f}, 0.7f * (1.0f - ph));
            }
            const float roll = 0.5f + 0.5f * std::sin(t * 1.3f + w.seed * 6.0f);
            const float cx = p.x - f * (0.75f + 0.9f * roll) * s;
            h.add(STRATA_PRIMITIVE_CUBE, cx, p.y + 0.32f * s, z + 0.15f, 0.62f * s, 0.34f * s, 0.46f * s, {0.42f, 0.22f, 0.12f});
            h.add(STRATA_PRIMITIVE_SPHERE, cx - 0.2f * s, p.y + 0.11f * s, z + 0.4f, 0.2f * s, 0.2f * s, 0.06f * s, kIron);
            h.add(STRATA_PRIMITIVE_SPHERE, cx + 0.2f * s, p.y + 0.11f * s, z + 0.4f, 0.2f * s, 0.2f * s, 0.06f * s, kIron);
            for (int k = 0; k < 3; ++k)
                h.add(STRATA_PRIMITIVE_SPHERE, cx + (k - 1) * 0.17f * s, p.y + 0.52f * s, z + 0.15f, 0.2f * s, 0.16f * s,
                      0.2f * s, k == 1 ? Rgb{0.75f, 0.6f, 0.2f} : Rgb{0.2f, 0.2f, 0.22f});
            break;
        }
        case AC_READ: {  // Holding the lamp up to a glittering vein.
            const float lx = p.x + f * 0.6f * s, ly = p.y + 1.15f * s;
            stick(h, p.x + f * 0.1f * s, p.y + 0.95f * s, lx, ly, z + 0.2f * s, p.shirt, 0.09f * s);
            h.add(STRATA_PRIMITIVE_SPHERE, lx, ly - 0.1f * s, z + 0.25f * s, 0.16f * s, 0.22f * s, 0.16f * s, {1.0f, 0.92f, 0.6f});
            h.point(lx, ly - 0.1f * s, z + 0.4f, 8.0f, 0.6f * s, kLampWarm, 0.9f);
            h.light(lx, ly, 0.8f, lightRange * 0.8f, kLampWarm, 1.6f, lightPriority);
            for (int k = 0; k < 7; ++k) {
                const float vy = p.y + (0.3f + k * 0.24f) * s, vz = 0.1f + 0.1f * k;
                const Rgb g = (k % 2) ? Rgb{0.95f, 0.75f, 0.2f} : Rgb{0.35f, 0.85f, 0.9f};
                h.add(STRATA_PRIMITIVE_SPHERE, w.wallX, vy, vz, 0.16f * s, 0.12f * s, 0.12f * s, g);
                const float tw = 0.5f + 0.5f * std::sin(t * 5.0f + k * 1.9f + w.seed * 9.0f);
                h.point(w.wallX - f * 0.05f, vy, vz + 0.1f, 3.0f + 5.0f * tw, 0.0f, g, tw, 0);
            }
            break;
        }
        case AC_WEB: {  // Pulling the bell rope that runs up to the surface.
            const float tug = 0.08f * s * std::sin(t * 5.0f);
            const float rx = handX, ry = handY + 0.25f * s + tug;
            stick(h, p.x, p.y + 1.0f * s, rx, ry, z + 0.2f * s, p.shirt, 0.09f * s);
            // Rope: up to the tunnel roof, along it to the shaft, then up the shaft.
            const float pts[4][2] = {{rx, ry}, {rx, w.ropeTurnY}, {w.ropeX, w.ropeTurnY}, {w.ropeX, 1.55f}};
            float total = 0.0f, segLen[3];
            for (int k = 0; k < 3; ++k) {
                segLen[k] = std::fabs(pts[k + 1][0] - pts[k][0]) + std::fabs(pts[k + 1][1] - pts[k][1]);
                total += segLen[k];
                h.line(pts[k][0], pts[k][1], z + 0.2f, pts[k + 1][0], pts[k + 1][1], z + 0.2f, {0.75f, 0.65f, 0.45f});
            }
            for (int n = 0; n < 4; ++n) {  // signals racing up the rope
                float d = frac(t * 0.7f + n / 4.0f) * total;
                for (int k = 0; k < 3; ++k) {
                    if (d <= segLen[k] || k == 2) {
                        const float u = segLen[k] > 0.0f ? std::min(d / segLen[k], 1.0f) : 0.0f;
                        h.point(pts[k][0] + (pts[k + 1][0] - pts[k][0]) * u, pts[k][1] + (pts[k + 1][1] - pts[k][1]) * u,
                                z + 0.3f, 9.0f, 0.18f, {0.55f, 0.85f, 1.0f}, 0.95f);
                        break;
                    }
                    d -= segLen[k];
                }
            }
            break;
        }
        case AC_DELEGATE: {  // Pointing as new tunnel mouths open in the walls.
            stick(h, p.x, p.y + 1.0f * s, p.x + f * 0.55f * s, p.y + 1.25f * s, z + 0.2f * s, p.shirt, 0.09f * s);
            for (int k = 0; k < 3; ++k) {
                const float ph = frac(t * 0.45f + k / 3.0f);
                const float side = (k % 2) ? -1.0f : 1.0f;
                const float mx = w.ropeX + side * (kShaftHalf - 0.02f), my = p.y + (0.7f + 0.55f * k) * s + 2.0f;
                ring(h, mx, my, kBack + 0.1f, 0.12f + 0.45f * ph, {0.95f, 0.8f, 0.45f}, 1.0f - ph, 14);
                h.point(mx, my, kBack + 0.2f, 10.0f * ph + 2.0f, 0.4f * ph, {0.5f, 0.45f, 0.38f}, 0.6f * (1.0f - ph));
            }
            break;
        }
        case AC_THINK: {  // Sitting on the upturned pick, thoughts rising.
            const float px = p.x + f * 0.42f * s;
            stick(h, px, p.y, px, p.y + 0.95f * s, z + 0.2f * s, kWood, 0.06f * s);
            stick(h, px - 0.3f * s, p.y + 0.95f * s, px + 0.3f * s, p.y + 0.98f * s, z + 0.2f * s, {0.55f, 0.56f, 0.6f}, 0.08f * s);
            for (int k = 0; k < 3; ++k) {
                const float ph = frac(t * 0.3f + k / 3.0f);
                h.point(p.x - f * 0.1f * s + 0.2f * std::sin(ph * 6.0f), p.y + (1.5f + ph * 1.3f) * s, z + 0.3f,
                        4.0f + 10.0f * ph, 0.08f + 0.22f * ph, {0.95f, 0.95f, 1.0f}, 0.75f * (1.0f - ph * ph));
            }
            break;
        }
        default: {  // Other: shovelling spoil over the shoulder.
            const float ph = frac(t * 0.9f + w.seed * 2.0f);
            const float a = (ph < 0.5f ? ph * 2.0f : 2.0f - ph * 2.0f) * 70.0f - 25.0f;
            const float r = a * kPi / 180.0f;
            const float ex = handX + f * std::cos(r) * 0.7f * s, ey = handY + std::sin(r) * 0.7f * s;
            stick(h, handX, handY, ex, ey, z + 0.25f * s, kWood, 0.05f * s);
            h.add(STRATA_PRIMITIVE_CUBE, ex, ey, z + 0.25f * s, 0.2f * s, 0.06f * s, 0.2f * s, kIron);
            if (ph > 0.5f) {
                const float age = (ph - 0.5f) * 1.1f;
                for (int k = 0; k < 6; ++k) {
                    const float vx = -f * (0.8f + hash1(k * 2.3f)), vy = 1.6f + hash1(k * 4.1f);
                    h.point(ex + vx * age, ey + vy * age - 4.9f * age * age, z + 0.3f, 5.0f, 0.1f,
                            {0.4f, 0.28f, 0.16f}, 1.0f, 0);
                }
            }
        }
        }
    }
    lampGlow(h, lamp, lampLevel, tint, s);
    if (lampLevel > 0.05f)
        h.light(lamp[0] + f * 0.3f, lamp[1], 0.7f, lightRange * (0.45f + 0.55f * lampLevel), mix(kLampWarm, tint, 0.25f),
                0.4f + 1.4f * lampLevel, lightPriority);
}

void selectionMark(Host& h, float x, float y, float s, float t) {
    const Rgb c{1.0f, 0.95f, 0.55f};
    const float x0 = x - 0.7f * s, x1 = x + 0.7f * s, y0 = y - 0.05f, y1 = y + 1.9f * s, z = 0.8f, k = 0.3f * s;
    const float corners[4][2] = {{x0, y0}, {x1, y0}, {x0, y1}, {x1, y1}};
    for (const auto& cn : corners) {
        const float sx = cn[0] == x0 ? 1.0f : -1.0f, sy = cn[1] == y0 ? 1.0f : -1.0f;
        stick(h, cn[0], cn[1], cn[0] + sx * k, cn[1], z, c, 0.05f);
        stick(h, cn[0], cn[1], cn[0], cn[1] + sy * k, z, c, 0.05f);
    }
    const float bob = 0.12f * std::sin(t * 4.0f);
    h.point(x, y1 + 0.35f + bob, z, 12.0f, 0.3f * s, c, 0.95f);
}

// ------------------------------------------------------------------ structures

void drawHeadframe(Host& h, const float* pl, float t) {
    const float x = pl[0], seed = pl[2], busy = pl[4], appear = pl[5];
    if (appear < 0.02f) return;
    const float top = 6.0f * appear, z = 0.1f;
    stick(h, x - 1.3f, 0.1f, x, top, z, kWood, 0.16f);
    stick(h, x + 1.3f, 0.1f, x, top, z, kWood, 0.16f);
    stick(h, x - 1.3f, 0.1f, x, top, kBack - 0.6f, kWoodDark, 0.16f);
    stick(h, x + 1.3f, 0.1f, x, top, kBack - 0.6f, kWoodDark, 0.16f);
    for (int k = 1; k <= 2; ++k) {
        const float yy = top * k / 3.0f, half = 1.3f * (1.0f - static_cast<float>(k) / 3.0f);
        stick(h, x - half, yy, x + half, yy, z, kWood, 0.1f);
    }
    // The sheave wheel, turning while anyone below is busy.
    const float wy = top + 0.4f, spin = t * 1.6f * busy;
    ring(h, x, wy, z + 0.05f, 0.7f, {0.2f, 0.2f, 0.22f}, 1.0f, 24);
    for (int k = 0; k < 4; ++k) {
        const float a = spin + k * kPi / 4.0f;
        stick(h, x - std::cos(a) * 0.68f, wy - std::sin(a) * 0.68f, x + std::cos(a) * 0.68f, wy + std::sin(a) * 0.68f,
              z + 0.05f, {0.25f, 0.25f, 0.28f}, 0.05f);
    }
    // A pennant in the place's own colour.
    const Rgb flag = shirtOf(seed);
    stick(h, x, wy + 0.7f, x, wy + 1.6f, z, kWoodDark, 0.05f);
    const float flap = 0.08f * std::sin(t * 3.0f + seed * 10.0f);
    h.add(STRATA_PRIMITIVE_CUBE, x + 0.32f, wy + 1.4f + flap, z, 0.6f, 0.34f, 0.04f, flag);
    // Winding house behind it, smoking while busy.
    h.add(STRATA_PRIMITIVE_CUBE, x, 0.75f, kBack - 1.8f, 2.6f, 1.5f, 1.8f, {0.55f, 0.42f, 0.32f});
    h.add(STRATA_PRIMITIVE_CUBE, x, 1.6f, kBack - 1.8f, 2.9f, 0.25f, 2.1f, {0.3f, 0.16f, 0.12f});
    h.add(STRATA_PRIMITIVE_CYLINDER, x + 0.9f, 2.2f, kBack - 2.2f, 0.3f, 0.7f, 0.3f, {0.35f, 0.25f, 0.2f});
    if (busy > 0.05f)
        for (int k = 0; k < 5; ++k) {
            const float ph = frac(t * 0.25f + k / 5.0f + seed);
            h.point(x + 0.9f + ph * 1.2f, 2.9f + ph * 2.5f, kBack - 2.2f, 6.0f + 18.0f * ph, 0.3f + 0.9f * ph,
                    {0.6f, 0.6f, 0.62f}, 0.45f * busy * (1.0f - ph));
        }
}

void drawShaft(Host& h, const float* s, float t) {
    const float x = s[0], face = s[1], seed = s[4], offline = s[8], appear = s[10];
    const bool underHeadframe = s[12] > 0.5f;
    // Collar and, unless the place's headframe stands over it, a little headgear.
    h.add(STRATA_PRIMITIVE_CUBE, x - kShaftHalf - 0.12f, 0.15f, 0.2f, 0.24f, 0.3f, 1.3f, kWood);
    h.add(STRATA_PRIMITIVE_CUBE, x + kShaftHalf + 0.12f, 0.15f, 0.2f, 0.24f, 0.3f, 1.3f, kWood);
    if (!underHeadframe) {
    stick(h, x - kShaftHalf - 0.1f, 0.3f, x - kShaftHalf - 0.1f, 1.7f, 0.0f, kWood, 0.12f);
    stick(h, x + kShaftHalf + 0.1f, 0.3f, x + kShaftHalf + 0.1f, 1.7f, 0.0f, kWood, 0.12f);
    stick(h, x - kShaftHalf - 0.25f, 1.7f, x + kShaftHalf + 0.25f, 1.7f, 0.0f, kWood, 0.14f);
    }
    h.add(STRATA_PRIMITIVE_CUBE, x, underHeadframe ? 0.45f : 1.86f, 0.55f, 0.4f, 0.2f, 0.2f, shirtOf(seed));  // the session's colour tag
    // Timber sets down the shaft, a ladder on one side.
    for (float y = -1.2f; y > face + 0.6f; y -= 1.8f)
        h.add(STRATA_PRIMITIVE_CUBE, x, y, kBack + 0.08f, kShaftHalf * 2.0f, 0.14f, 0.12f, kWoodDark);
    const float lx = x - kShaftHalf + 0.18f;
    h.line(lx, 0.5f, 0.0f, lx, face, 0.0f, {0.45f, 0.3f, 0.15f});
    h.line(lx + 0.32f, 0.5f, 0.0f, lx + 0.32f, face, 0.0f, {0.45f, 0.3f, 0.15f});
    for (float y = 0.3f; y > face; y -= 0.4f) h.line(lx, y, 0.0f, lx + 0.32f, y, 0.0f, {0.45f, 0.3f, 0.15f});
    // Chamber rails.
    h.line(x - kChamberHalf, face + 0.03f, 0.05f, x + kChamberHalf, face + 0.03f, 0.05f, kIron);
    h.line(x - kChamberHalf, face + 0.03f, 0.45f, x + kChamberHalf, face + 0.03f, 0.45f, kIron);
    if (offline > 0.02f) {
        // Boarded up: planks across the collar and a cross of boards below it.
        for (int k = 0; k < 3; ++k)
            h.add(STRATA_PRIMITIVE_CUBE, x, 0.4f + 0.4f * k, 0.55f, kShaftHalf * 2.6f, 0.22f, 0.08f,
                  mul(kWood, 0.7f + 0.1f * k), offline);
        stick(h, x - kShaftHalf, -0.3f, x + kShaftHalf, -2.2f, 0.6f, mul(kWood, 0.8f), 0.18f);
        stick(h, x + kShaftHalf, -0.3f, x - kShaftHalf, -2.2f, 0.6f, mul(kWood, 0.8f), 0.18f);
        for (int k = 0; k < 6; ++k)  // rubble in the chamber
            h.add(STRATA_PRIMITIVE_SPHERE, x - 1.3f + k * 0.5f, face + 0.18f + 0.1f * hash1(k + seed), 0.2f,
                  0.5f, 0.36f + 0.2f * hash1(k * 3.0f + seed), 0.5f, {0.3f, 0.27f, 0.25f});
    }
    (void)appear;
    (void)t;
}

void drawTunnel(Host& h, const float* f) {
    const float sx = f[0], y = f[1], side = f[2], len = f[3];
    const float a = sx + side * kShaftHalf, b = sx + side * (kShaftHalf + len);
    // Props every so often, rails along the floor.
    for (float d = 0.9f; d < len - 0.3f; d += 1.5f) {
        const float px = sx + side * (kShaftHalf + d);
        h.add(STRATA_PRIMITIVE_CUBE, px, y + kTunnelHigh * 0.5f, kBack + 0.12f, 0.14f, kTunnelHigh, 0.14f, kWoodDark);
        h.add(STRATA_PRIMITIVE_CUBE, px, y + kTunnelHigh - 0.08f, 0.1f, 0.3f, 0.14f, 1.3f, kWood);
    }
    h.line(a, y + 0.03f, 0.05f, b, y + 0.03f, 0.05f, kIron);
    h.line(a, y + 0.03f, 0.45f, b, y + 0.03f, 0.45f, kIron);
}

void drawCage(Host& h, float x, float level, float t) {
    // The lift cage at the surface, rattling, with a red flare overhead.
    if (level < 0.02f) return;
    const float jig = 0.04f * std::sin(t * 30.0f) * level;
    const float cx = x + jig, y0 = 0.35f, y1 = 1.5f, hw = 0.55f;
    const Rgb bar{0.75f, 0.12f, 0.08f};
    for (int k = 0; k <= 4; ++k) {
        const float bx = cx - hw + k * hw * 0.5f;
        stick(h, bx, y0, bx, y1, 0.55f, bar, 0.05f);
    }
    stick(h, cx - hw, y0, cx + hw, y0, 0.55f, bar, 0.08f);
    stick(h, cx - hw, y1, cx + hw, y1, 0.55f, bar, 0.08f);
    const float pulse = 0.5f + 0.5f * std::sin(t * 6.0f);
    // A flare rises and bursts over the shaft, again and again.
    const float ph = frac(t * 0.45f);
    const float fy = 2.2f + 4.5f * std::sin(std::min(ph, 0.5f) * kPi);
    h.point(x, fy, 0.6f, 22.0f, 0.9f + 1.0f * pulse, kRed, level);
    if (ph > 0.5f) {
        const float age = ph - 0.5f;
        for (int k = 0; k < 12; ++k) {
            const float a = k * 2 * kPi / 12;
            h.point(x + std::cos(a) * age * 4.0f, fy + std::sin(a) * age * 4.0f - age * age * 3.0f, 0.6f, 6.0f, 0.15f,
                    {1.0f, 0.35f, 0.2f}, level * (1.0f - age * 2.0f), 0);
        }
    }
    h.point(x, 1.95f, 0.7f, 22.0f, 1.2f + 0.8f * pulse, kRed, level * (0.6f + 0.4f * pulse));
    // A pillar of red light standing over the shaft, seen from anywhere.
    for (int k = 0; k < 14; ++k) {
        const float yy = 2.4f + k * 0.8f, w = 1.0f - k / 14.0f;
        h.point(x, yy, 0.4f, 10.0f, 0.9f * w + 0.3f, kRed, level * w * (0.35f + 0.35f * pulse));
    }
    h.add(STRATA_PRIMITIVE_SPHERE, x, 1.95f, 0.3f, 0.3f, 0.3f, 0.3f, mix({0.4f, 0.05f, 0.03f}, {1.0f, 0.2f, 0.1f}, pulse));
    h.light(x, 2.4f, 1.2f, 9.0f, kRed, (1.5f + 2.5f * pulse) * level, 20);
    // The red signal running down the shaft to the miner who rang.
}

void drawStalled(Host& h, float x, float y, float level, float t, float seed) {
    if (level < 0.02f) return;
    // Dust churning round the face, pebbles dropping, an amber warning glow.
    for (int k = 0; k < 18; ++k) {
        const float a = t * (0.6f + 0.3f * hash1(k + seed)) + k * 2.4f;
        const float r = 0.4f + 1.2f * hash1(k * 1.7f + seed);
        h.point(x + std::cos(a) * r, y + 1.0f + std::sin(a * 0.7f) * 0.8f, 0.3f, 22.0f, 0.9f, {0.62f, 0.56f, 0.46f},
                0.5f * level);
    }
    for (int k = 0; k < 4; ++k) {
        const float ph = frac(t * 0.8f + k / 4.0f);
        h.add(STRATA_PRIMITIVE_CUBE, x - 1.0f + k * 0.6f, y + 2.2f - ph * 2.2f, 0.2f, 0.1f, 0.1f, 0.1f, {0.35f, 0.3f, 0.26f});
    }
    const float flick = 0.6f + 0.4f * std::sin(t * 9.0f) * std::sin(t * 3.7f);
    h.light(x, y + 1.6f, 0.8f, 7.0f, {1.0f, 0.55f, 0.1f}, 2.6f * flick * level, 12);
    // A warning lamp blinking amber over the face.
    h.add(STRATA_PRIMITIVE_SPHERE, x, y + 2.05f, kBack + 0.3f, 0.22f, 0.22f, 0.22f, mix({0.3f, 0.15f, 0.02f}, {1.0f, 0.6f, 0.1f}, flick));
    h.point(x, y + 2.05f, 0.5f, 18.0f, 0.8f, {1.0f, 0.6f, 0.15f}, 0.85f * flick * level);
}

}  // namespace

void strata_kit_configure(strata_kit::Options& o) {
    o.name = "agentviz-mine";
    o.tiers[0].maxLights = 6;
    o.tiers[1].maxLights = 12;
    o.tiers[2].maxLights = 24;
    o.farPlane = 500.0f;
}

void strata_kit_engine_ready(strata_kit::Host&) {}

void strata_kit_build(strata_kit::Host& h, double seconds) {
    const float t = h.reducedMotion ? 0.0f : static_cast<float>(seconds);
    h.sunIntensity = 0.7f;
    h.ambientLift = {0.03f, 0.025f, 0.02f};
    collectCavities();
    drawEarth(h);

    for (size_t i = 0; i + kPlaceStride <= places.size(); i += kPlaceStride) drawHeadframe(h, &places[i], t);

    for (size_t i = 0; i + kFollowerStride <= followers.size(); i += kFollowerStride) {
        const float* f = &followers[i];
        drawTunnel(h, f);
        const float sx = f[0], y = f[1], side = f[2], len = f[3], appear = f[8];
        if (appear < 0.05f || f[10] > 0.5f) continue;
        const float wallX = sx + side * (kShaftHalf + len);
        MinerPose p{wallX - side * 0.65f, y, 0.15f, side, 0.8f, shirtOf(f[9]), false};
        Work w{static_cast<int>(f[4] + 0.5f), t, f[6], wallX, sx, y + kTunnelHigh - 0.15f, f[5] < 0.5f, false, false};
        const bool sel = f[7] > 0.5f;
        drawAt(h, p, w, f[5] > 0.5f ? 0.9f : 0.2f, appear, shirtOf(f[9]), sel ? 15 : (f[5] > 0.5f ? 3 : 2), 5.0f);
        if (sel) selectionMark(h, p.x, y, 0.8f, t);
    }

    for (size_t i = 0; i + kSessionStride <= sessions.size(); i += kSessionStride) {
        const float* s = &sessions[i];
        drawShaft(h, s, t);
        const float x = s[0], face = s[1], seed = s[4], lamp = s[5], waiting = s[6], stalled = s[7], offline = s[8];
        const int state = static_cast<int>(s[2] + 0.5f);
        const bool sel = s[9] > 0.5f;
        drawCage(h, x, waiting, t);
        if (offline > 0.5f) {
            if (sel) selectionMark(h, x, 0.3f, 1.0f, t);
            continue;
        }
        // The miner works the chamber's face, on the side away from the ladder.
        const float facing = s[11] < 0.0f ? -1.0f : 1.0f;
        const float wallX = x + facing * kChamberHalf;
        MinerPose p{wallX - facing * 0.85f, face, 0.15f, facing, 1.0f, shirtOf(seed), false};
        Work w{static_cast<int>(s[3] + 0.5f), t, seed, wallX, x, face + kChamberHigh - 0.2f, false,
               state == ST_IDLE || state == ST_DIM, waiting > 0.5f};
        drawAt(h, p, w, lamp, 1.0f - offline, shirtOf(seed), sel ? 16 : (waiting > 0.5f ? 14 : 6), 7.0f);
        if (waiting > 0.02f) {
            // A red signal running up the shaft from the miner to the cage.
            for (int k = 0; k < 5; ++k) {
                const float ph = frac(t * 0.9f + k / 5.0f);
                h.point(x + facing * 0.3f, face + 1.8f + ph * (-face - 0.4f), 0.4f, 8.0f, 0.22f, kRed, waiting * (1.0f - ph * 0.5f));
            }
        }
        drawStalled(h, p.x, face, stalled, t, seed);
        if (sel) selectionMark(h, p.x, face, 1.0f, t);
    }
}

// ------------------------------------------------------------------ setters

// World bounds: the x span of the workings and the deepest floor (negative y).
STRATA_KIT_EXPORT void mn_set_world(float minX, float maxX, float bottom) {
    worldMin = minX;
    worldMax = maxX;
    worldBottom = bottom;
}

// Places: n rows of [x, halfWidth, seed, waiting, busy, appear].
STRATA_KIT_EXPORT void mn_set_places(const float* rows, int n) { places.assign(rows, rows + n * kPlaceStride); }

// Sessions: n rows of [x, faceY, state, action, seed, lamp, waiting, stalled,
// offline, selected, appear, facing (+1/-1), underHeadframe, -]. State and action codes as above.
STRATA_KIT_EXPORT void mn_set_sessions(const float* rows, int n) { sessions.assign(rows, rows + n * kSessionStride); }

// Followers: n rows of [shaftX, floorY, side, length, action, active, seed,
// selected, appear, parentSeed, parentOffline, -].
STRATA_KIT_EXPORT void mn_set_followers(const float* rows, int n) {
    followers.assign(rows, rows + n * kFollowerStride);
}
