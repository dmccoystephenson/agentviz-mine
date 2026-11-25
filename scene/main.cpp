// agentviz-mine: Claude Code sessions as a mine, seen in cross-section.
//
// The page (web/mine-*.js, web/page.js) owns the data and the layout: it
// reads the agent feed, places every pithead (place), shaft (session) and
// tunnel (subagent), eases between snapshots and, every frame, packs the
// scene in the agentviz wire format (vendor/agentviz-kit, docs/KIT.md in the
// agentviz repo) for agentviz_frame(). This file only draws what the frame
// says, with earth.h, structures.h and miner.h.
#define STRATA_KIT_MAIN
#include "strata_kit.h"

#include "earth.h"
#include "miner.h"
#include "structures.h"

namespace {

using namespace mine;
using agentviz::State;

agentviz::Frame frame;

void drawSubagent(Host& h, const Tunnel& t, float time) {
    drawTunnel(h, t);
    if (t.appear < 0.05f || t.leadOffline > 0.5f) return;
    const float wallX = tunnelEnd(t);
    const Rgb shirt = shirtOf(t.leadSeed);
    const MinerPose pose{wallX - t.side * 0.65f, t.y, 0.15f, t.side, 0.8f, shirt, false};
    const Work work{t.action, time, t.seed, wallX, t.sx, t.y + kTunnelHigh - 0.15f, t.pulse, t.active < 0.5f, false, false};
    const Light light{t.active > 0.5f ? 0.9f : 0.2f, t.appear, shirt, t.selected ? 15 : (t.active > 0.5f ? 3 : 2), 5.0f};
    drawAt(h, pose, work, light);
    drawStalled(h, pose.x, t.y, t.stalled, time, t.seed, 0.75f);
    if (t.selected) selectionMark(h, pose.x, t.y, 0.8f, time);
}

void drawSession(Host& h, const Lead& l, float time) {
    drawShaft(h, l);
    drawCage(h, l, time);
    if (l.offline > 0.5f) {
        if (l.selected) selectionMark(h, l.x, 0.3f, 1.0f, time);
        return;
    }
    // The miner works the chamber's face on the side away from the ladder.
    const float wallX = l.x + l.facing * kChamberHalf;
    const Rgb shirt = shirtOf(l.seed);
    const bool waiting = l.waiting > 0.5f;
    const MinerPose pose{wallX - l.facing * 0.85f, l.face, 0.15f, l.facing, 1.0f, shirt, false};
    const Work work{l.action, time, l.seed, wallX, l.x, l.face + kChamberHigh - 0.2f, l.pulse, false,
                    l.state == State::Idle || l.state == State::Dim, waiting};
    const Light light{l.lamp, 1.0f - l.offline, shirt, l.selected ? 16 : (waiting ? 14 : 6), 7.0f};
    drawAt(h, pose, work, light);
    drawSignal(h, l, time);
    drawStalled(h, pose.x, l.face, l.stalled, time, l.seed);
    if (l.selected) selectionMark(h, pose.x, l.face, 1.0f, time);
}

}  // namespace

AGENTVIZ_FRAME_EXPORT(frame)

void strata_kit_configure(strata_kit::Options& o) {
    o.name = "agentviz-mine";
    o.tiers[0].maxLights = 6;
    o.tiers[1].maxLights = 12;
    o.tiers[2].maxLights = 24;
    o.farPlane = 500.0f;
}

void strata_kit_engine_ready(strata_kit::Host&) {}

void strata_kit_build(strata_kit::Host& h, double seconds) {
    if (!frame.ok) return;
    const float t = h.reducedMotion ? 0.0f : static_cast<float>(seconds);
    h.sunIntensity = 0.7f;
    h.ambientLift = {0.03f, 0.025f, 0.02f};
    std::vector<Lead> leads;
    std::vector<Tunnel> tunnels;
    for (const agentviz::Agent& a : frame.agents) {
        if (a.lead()) leads.push_back(leadOf(a));
        else tunnels.push_back(tunnelOf(a));
    }
    drawEarth(h, worldOf(frame), cavitiesOf(leads, tunnels));
    for (const agentviz::Zone& z : frame.zones) drawHeadframe(h, pitheadOf(z), t);
    for (const Tunnel& tunnel : tunnels) drawSubagent(h, tunnel, t);
    for (const Lead& lead : leads) drawSession(h, lead, t);
}
