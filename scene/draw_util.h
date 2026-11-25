// draw_util.h: the mine's dimensions, colours and small drawing helpers.
#pragma once

#include <algorithm>
#include <cmath>

#include "strata_kit.h"

namespace mine {

using strata_kit::Host;
using strata_kit::Rgb;

constexpr float kPi = 3.14159265f;

// The cut face of the earth runs from z = kBack (the back of every cavity) to
// z = kFront (the cross-section's face, nearest the camera). The page lays
// shafts out with the same half widths (web/mine-model.js).
constexpr float kBack = -0.55f, kFront = 0.9f;
constexpr float kShaftHalf = 0.8f, kChamberHalf = 1.9f, kChamberHigh = 2.3f, kTunnelHigh = 1.8f;

const Rgb kShirts[8] = {{0.85f, 0.30f, 0.22f}, {0.12f, 0.60f, 0.58f}, {0.85f, 0.65f, 0.12f}, {0.50f, 0.30f, 0.78f},
                        {0.45f, 0.72f, 0.20f}, {0.20f, 0.48f, 0.85f}, {0.80f, 0.25f, 0.60f}, {0.92f, 0.48f, 0.14f}};
const Rgb kWood{0.36f, 0.22f, 0.10f}, kWoodDark{0.22f, 0.13f, 0.06f}, kIron{0.30f, 0.31f, 0.34f};
const Rgb kSteel{0.55f, 0.56f, 0.60f};
const Rgb kSkin{0.78f, 0.56f, 0.42f}, kTrousers{0.16f, 0.18f, 0.24f}, kHelmet{0.88f, 0.70f, 0.14f};
const Rgb kLampWarm{1.0f, 0.86f, 0.55f}, kRed{1.0f, 0.12f, 0.08f}, kAmber{1.0f, 0.6f, 0.15f};
const Rgb kGold{0.95f, 0.75f, 0.2f}, kCyan{0.35f, 0.85f, 0.9f};

inline float hash1(float n) {
    const float s = std::sin(n * 127.1f + 311.7f) * 43758.5453f;
    return s - std::floor(s);
}
inline float frac(float v) { return v - std::floor(v); }
inline Rgb mul(Rgb c, float k) { return {c.r * k, c.g * k, c.b * k}; }
inline Rgb mix(Rgb a, Rgb b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
// The page colours label edges with the same table (web/page.js SHIRTS).
inline Rgb shirtOf(float seed) { return kShirts[std::clamp(static_cast<int>(seed * 8.0f), 0, 7)]; }

// A flat stick in the plane z (a wide line, `width` world units).
inline void stick(Host& h, float x0, float y0, float x1, float y1, float z, Rgb c, float width) {
    h.wideLine(x0, y0, z, x1, y1, z, c, 2.0f, width);
}

inline void ring(Host& h, float x, float y, float z, float r, Rgb c, float a, int n = 20) {
    for (int k = 0; k < n; ++k) {
        const float a0 = k * 2 * kPi / n, a1 = (k + 1) * 2 * kPi / n;
        h.line(x + std::cos(a0) * r, y + std::sin(a0) * r, z, x + std::cos(a1) * r, y + std::sin(a1) * r, z, c, a);
    }
}

}  // namespace mine
