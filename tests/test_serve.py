"""Serve the fixture feed and check the page, its assets and the feed."""
import json
import os
import socket
import subprocess
import sys
import time
import unittest
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = int(os.environ.get("MINE_TEST_PORT", "8945"))
BASE = "http://127.0.0.1:%d" % PORT


def fetch(path):
    with urllib.request.urlopen(BASE + path, timeout=10) as r:
        return r.status, r.headers.get("Content-Type", ""), r.read()


class ServeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.proc = subprocess.Popen(
            [sys.executable, "-m", "vendor.agentfeed", "serve", "--web", "web", "--port", str(PORT), "--fixture"],
            cwd=ROOT, stdout=subprocess.DEVNULL)
        for _ in range(100):
            try:
                socket.create_connection(("127.0.0.1", PORT), timeout=0.2).close()
                return
            except OSError:
                time.sleep(0.1)
        cls.proc.kill()
        raise RuntimeError("server did not start")

    @classmethod
    def tearDownClass(cls):
        cls.proc.terminate()
        cls.proc.wait(5)

    def test_page_and_assets(self):
        status, _, body = fetch("/")
        self.assertEqual(status, 200)
        page = body.decode()
        for asset in ("strata-loader.js", "strata-kit.js", "mine.js"):
            self.assertIn(asset, page)
        for asset, ctype in (("/mine.js", "javascript"), ("/strata-kit.js", "javascript"),
                             ("/strata-loader.js", "javascript"), ("/mine-engine.js", "javascript"),
                             ("/mine-engine.wasm", "application/wasm")):
            status, got, body = fetch(asset)
            self.assertEqual(status, 200, asset)
            self.assertIn(ctype, got, asset)
            self.assertGreater(len(body), 1000, asset)

    def test_page_uses_scene_exports(self):
        js = fetch("/mine.js")[2].decode()
        for name in ("_mn_set_world", "_mn_set_places", "_mn_set_sessions", "_mn_set_followers", "/api/agents"):
            self.assertIn(name, js)
        conf = open(os.path.join(ROOT, "scene", "kit.conf")).read()
        for name in ("_mn_set_world", "_mn_set_places", "_mn_set_sessions", "_mn_set_followers"):
            self.assertIn(name, conf)

    def test_feed_parses(self):
        status, ctype, body = fetch("/api/agents?fixture=1")
        self.assertEqual(status, 200)
        self.assertIn("json", ctype)
        doc = json.loads(body)
        self.assertEqual(doc["version"], 1)
        self.assertTrue(doc["fixture"])
        states = {s["state"] for s in doc["sessions"]}
        self.assertTrue({"busy", "idle", "waiting", "offline"} <= states)
        for s in doc["sessions"]:
            for key in ("id", "name", "state", "action", "place", "followers", "seed"):
                self.assertIn(key, s)
        self.assertEqual(sum(len(s["followers"]) for s in doc["sessions"]), 11)

    def test_outside_web_is_404(self):
        try:
            fetch("/../vendor/agentfeed/feed.py")
            self.fail("expected 404")
        except urllib.error.HTTPError as e:
            self.assertEqual(e.code, 404)


if __name__ == "__main__":
    unittest.main()
