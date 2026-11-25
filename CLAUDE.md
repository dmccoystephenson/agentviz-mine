# CLAUDE.md

agentviz-mine: Claude Code sessions drawn as a mine cross-section on the
Strata web kit. Part of the agentviz family (https://github.com/dmccoystephenson/agentviz).

- Data comes only from `/api/agents` (vendor/agentfeed, schema in its docs).
  Never edit `vendor/agentfeed/`; re-vendor with agentviz's `tools/vendor-feed.sh`.
- Scene: `scene/main.cpp` (draws), `web/mine.js` (layout, easing, picking, panel).
  Rebuild with `scene/build.sh` (Strata 0.10.0, emsdk 6.0.11); the built
  `web/mine-engine.js|.wasm` are committed.
- Run: `python3 -m vendor.agentfeed serve --web web --port 8845 [--fixture]`.
- Tests: `python3 -m unittest discover -s tests`; browser smoke:
  `NODE_PATH=/root/arcade-watch/node_modules node tests/smoke.cjs`.
- Screenshots only from the fixture feed. Port 8845 (tests use 8945).
- Private repo; hosted Actions are refused, run localci on PRs.
