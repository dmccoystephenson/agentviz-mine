// earth.h: the banded earth cut away round the workings, the land behind the
// cut and bedrock under it. Every shaft, chamber and tunnel is a cavity left
// out of the rock.
#pragma once

#include <utility>
#include <vector>

#include "workings.h"

namespace mine {

// Strata bands, top to bottom, cycled with depth.
struct Band {
    float thick;
    Rgb c;
};
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
constexpr float kColumnWidth = 1.2f;
const Rgb kGrass{0.22f, 0.36f, 0.12f}, kCavityBack{0.10f, 0.075f, 0.055f};

struct Cavity {
    float x0, x1, y0, y1;
};
// A vertical gap in one column of rock: (bottom y, top y).
using Hole = std::pair<float, float>;

inline std::vector<Cavity> cavitiesOf(const std::vector<Lead>& leads, const std::vector<Tunnel>& tunnels) {
    std::vector<Cavity> out;
    for (const Lead& l : leads) {
        out.push_back({l.x - kShaftHalf, l.x + kShaftHalf, l.face, 0.6f});
        out.push_back({l.x - kChamberHalf, l.x + kChamberHalf, l.face, l.face + kChamberHigh});
    }
    for (const Tunnel& t : tunnels) {
        const float a = t.sx + t.side * (kShaftHalf - 0.05f), b = tunnelEnd(t);
        out.push_back({std::min(a, b), std::max(a, b), t.y, t.y + kTunnelHigh});
    }
    return out;
}

// Depth (positive, below the surface) where band boundary k lies at x.
inline float boundaryDepth(int k, float x) {
    if (k == 0) return 0.0f;
    float d = 0.0f;
    for (int i = 0; i < k; ++i) d += kBands[i % kBandCount].thick;
    return d + 0.45f * std::sin(x * 0.11f + k * 1.7f) + 0.22f * std::sin(x * 0.31f + k * 2.9f);
}

// Rock from y = hi down to y = lo in the column [a, b], one block per band.
inline void drawRock(Host& h, float a, float b, float hi, float lo) {
    if (hi - lo < 0.01f) return;
    const float cx = 0.5f * (a + b);
    for (int k = 0; k < 40; ++k) {
        const float d0 = boundaryDepth(k, cx), d1 = boundaryDepth(k + 1, cx);
        const float y0 = std::min(hi, -d0), y1 = std::max(lo, -d1);
        if (y0 - y1 > 0.005f) {
            const float depth = std::clamp(-0.5f * (y0 + y1) / 24.0f, 0.0f, 1.0f);
            const float jitter = 0.92f + 0.16f * hash1(cx * 3.1f + k * 7.7f);
            const Rgb c = mul(kBands[k % kBandCount].c, (1.0f - 0.45f * depth) * jitter);
            h.add(STRATA_PRIMITIVE_CUBE, cx, 0.5f * (y0 + y1), 0.5f * (kBack + kFront), b - a, y0 - y1, kFront - kBack, c);
        }
        if (-d1 < lo) break;
    }
}

// A glint of ore in the cut face, now and then, clear of any hole.
inline void drawOreGlint(Host& h, float cx, float bottom, const std::vector<Hole>& holes) {
    if (hash1(cx * 1.37f) <= 0.7f) return;
    const float gy = -2.0f - hash1(cx * 9.1f) * (-bottom - 3.0f);
    for (const Hole& hole : holes)
        if (gy > hole.first - 0.3f && gy < hole.second + 0.3f) return;
    const Rgb g = hash1(cx * 4.3f) > 0.5f ? Rgb{0.9f, 0.72f, 0.2f} : Rgb{0.3f, 0.75f, 0.8f};
    h.add(STRATA_PRIMITIVE_SPHERE, cx, gy, kFront, 0.16f, 0.12f, 0.08f, g);
}

// One column: walk down from the surface, rock between the holes, grass on
// top unless a hole breaks the surface.
inline void drawColumn(Host& h, float a, float b, float bottom, const std::vector<Cavity>& cavities) {
    const float cx = 0.5f * (a + b);
    std::vector<Hole> holes;
    for (const Cavity& c : cavities)
        if (cx > c.x0 && cx < c.x1) holes.push_back({c.y0, c.y1});
    std::sort(holes.begin(), holes.end(), [](const Hole& p, const Hole& q) { return p.second > q.second; });
    float top = 0.0f;
    bool grass = true;
    for (const Hole& hole : holes) {
        if (hole.second >= top) grass = false;
        else drawRock(h, a, b, top, hole.second);
        top = std::min(top, hole.first);
    }
    drawRock(h, a, b, top, bottom);
    if (grass) h.add(STRATA_PRIMITIVE_CUBE, cx, 0.07f, 0.5f * (kBack + kFront), b - a, 0.16f, kFront - kBack, kGrass);
    drawOreGlint(h, cx, bottom, holes);
}

// Column edges: a regular grid plus every cavity's sides.
inline std::vector<float> columnEdges(float left, float right, const std::vector<Cavity>& cavities) {
    std::vector<float> xs;
    for (float x = left; x < right; x += kColumnWidth) xs.push_back(x);
    xs.push_back(right);
    for (const Cavity& c : cavities) {
        xs.push_back(c.x0);
        xs.push_back(c.x1);
    }
    std::sort(xs.begin(), xs.end());
    return xs;
}

inline void drawEarth(Host& h, const World& w, const std::vector<Cavity>& cavities) {
    const float left = w.minX - 10.0f, right = w.maxX + 10.0f, bottom = w.bottom - 3.0f;
    const std::vector<float> xs = columnEdges(left, right, cavities);
    for (size_t i = 0; i + 1 < xs.size(); ++i)
        if (xs[i + 1] - xs[i] >= 0.02f) drawColumn(h, xs[i], xs[i + 1], bottom, cavities);
    // The cavities' back walls, darker than the face.
    for (const Cavity& c : cavities)
        h.add(STRATA_PRIMITIVE_CUBE, 0.5f * (c.x0 + c.x1), 0.5f * (c.y0 + std::min(c.y1, 0.0f)), kBack - 0.05f,
              c.x1 - c.x0, std::min(c.y1, 0.0f) - c.y0, 0.1f, kCavityBack);
    // The land behind the cut, and bedrock under it.
    h.add(STRATA_PRIMITIVE_CUBE, 0.5f * (left + right), -0.2f, kBack - 40.0f, (right - left) + 120.0f, 0.4f, 80.0f,
          {0.20f, 0.33f, 0.11f});
    h.add(STRATA_PRIMITIVE_CUBE, 0.5f * (left + right), bottom - 30.0f, 0.5f * (kBack + kFront), right - left, 60.0f,
          kFront - kBack + 0.2f, {0.07f, 0.07f, 0.08f});
}

}  // namespace mine
