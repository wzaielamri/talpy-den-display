"""Tests for sync.py's pure parts: python3 -m unittest discover -s host -p 'test_*.py' (from the repository)."""
import importlib.util
import json
import sys
import types
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
if "serial" not in sys.modules:
    try:
        import serial  # noqa: F401
    except ImportError:  # (the tests never open a port: a stand-in will do without pyserial)
        fake = types.ModuleType("serial")
        fake.Serial = object
        fake.SerialException = OSError
        sys.modules["serial"] = fake

_spec = importlib.util.spec_from_file_location("sync", Path(__file__).resolve().parent / "sync.py")
sync = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sync)


def job(**fields):
    base = {"id": ".claude/0b1c/live", "name": "fix the parser", "state": "working", "since": 1794145000000,
            "host": "", "where": "local"}
    base.update(fields)
    return base


class JobsRowsTests(unittest.TestCase):
    def test_jobs_rows_started(self):
        rows = sync.jobs_rows({"jobs": [job(startedAt=1794145062163), job(id="b")]})
        self.assertEqual(rows[0]["started"], 1794145062)
        self.assertNotIn("started", rows[1])

    def test_fit_rows_counts_started(self):
        many = [job(id=f".claude/{i:032x}/live", name="n" * 40, startedAt=1794145062163) for i in range(200)]
        rows = sync.jobs_rows({"jobs": many})
        line = json.dumps({"type": "jobs", "updated": "14:52:05", "rows": rows}, separators=(",", ":"))
        self.assertLessEqual(len(line), sync.MAX_LINE)
        self.assertTrue(all("started" in r for r in rows))


class DemoTests(unittest.TestCase):
    def test_demo_rows(self):
        rows = sync.demo_rows(12, 1794145062)
        self.assertEqual(len(rows), 12)
        self.assertEqual({r["state"] for r in rows}, {"waiting", "working", "done"})
        self.assertTrue(any(r["machine"].endswith("(ssh)") for r in rows))
        self.assertTrue(any('"' in r["name"] and '"' in r["id"] for r in rows))  # (the board's JSON, escaped)
        self.assertTrue(all(r["started"] <= 1794145062 for r in rows))

    def test_demo_rows_64_fit_one_message(self):
        rows = sync.demo_rows(64, 1794145062)
        self.assertEqual(len(rows), 64)
        self.assertEqual(len({r["id"] for r in rows}), 64)
        line = json.dumps({"type": "jobs", "updated": "14:52:05", "rows": rows}, separators=(",", ":"))
        self.assertLessEqual(len(line), sync.MAX_LINE)


if __name__ == "__main__":
    unittest.main()
