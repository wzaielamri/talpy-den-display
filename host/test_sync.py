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


MONITOR_LINE = ("/org/freedesktop/login1/session/_32: org.freedesktop.DBus.Properties.PropertiesChanged "
                "('org.freedesktop.login1.Session', {'LockedHint': <%s>}, @as [])")


class FakeRun:
    """subprocess.run as logind would answer it: answers by the command's words; counts LockedHint reads."""

    def __init__(self, display="2", locked="no", table="", sessions=None, fail=False):
        self.display, self.locked, self.table, self.fail = display, locked, table, fail
        self.sessions = sessions or {}
        self.reads = 0

    def __call__(self, argv, **kwargs):
        if self.fail:
            raise FileNotFoundError(argv[0])
        out = ""
        if argv[:2] == ["loginctl", "show-user"]:
            out = self.display
        elif argv[:2] == ["loginctl", "list-sessions"]:
            out = self.table
        elif argv[:2] == ["loginctl", "show-session"]:
            prop = argv[argv.index("-p") + 1]
            if prop == "LockedHint":
                self.reads += 1
                out = self.locked
            else:
                out = self.sessions.get(argv[2], {}).get(prop, "")
        elif argv[0] == "gdbus" and "call" in argv:
            out = "(objectpath '/org/freedesktop/login1/session/_32',)"
        return types.SimpleNamespace(stdout=out + "\n", returncode=0)


class EndedMonitor:
    """A gdbus monitor that has already ended: its output is at its end."""

    def __init__(self, *args, **kwargs):
        import os
        r, w = os.pipe()
        os.close(w)
        self.stdout = open(r, "rb", buffering=0)

    def poll(self):
        return 0


class LockTests(unittest.TestCase):
    def test_parse_locked_hint(self):
        self.assertIs(sync.parse_locked_hint(MONITOR_LINE % "true"), True)
        self.assertIs(sync.parse_locked_hint(MONITOR_LINE % "false"), False)
        self.assertIsNone(sync.parse_locked_hint(MONITOR_LINE.replace("LockedHint", "IdleHint") % "true"))

    def test_display_session(self):
        self.assertEqual(sync.display_session(FakeRun(display="2")), "2")
        table = ("2 1000 me seat0 2387 user    tty2 no -\n"
                 "3 1000 me -     2393 manager -    no -\n")
        run = FakeRun(display="", table=table, sessions={"2": {"Type": "wayland", "Class": "user"},
                                                         "3": {"Type": "unspecified", "Class": "manager"}})
        self.assertEqual(sync.display_session(run), "2")
        self.assertIsNone(sync.display_session(FakeRun(display="", table="")))

    def test_lockwatch_without_logind_is_unlocked(self):
        watch = sync.LockWatch(run=FakeRun(fail=True), popen=EndedMonitor)
        self.assertFalse(watch.locked)
        self.assertIsNone(watch.fileno())
        self.assertFalse(watch.poll())

    def test_lockwatch_reads_every_2s_when_monitor_ends(self):
        now = [100.0]
        run = FakeRun(locked="no")
        watch = sync.LockWatch(run=run, popen=EndedMonitor, clock=lambda: now[0])
        self.assertFalse(watch.locked)
        self.assertFalse(watch.poll())  # (reads the end of the monitor's output: it's gone)
        self.assertIsNone(watch.fileno())
        run.locked = "yes"
        reads = run.reads
        changed = [watch.poll() for _ in range(5)]  # (the same second: one read at most)
        self.assertLessEqual(run.reads - reads, 1)
        now[0] += 2.1
        changed.append(watch.poll())
        self.assertTrue(any(changed))
        self.assertTrue(watch.locked)

    def test_send_all_sends_lock_first(self):
        sent = []
        board = types.SimpleNamespace(send=lambda type_, **fields: sent.append(type_))
        sync.send_everything(board, "CET-1", True, lambda: board.send("jobs"), None)
        self.assertEqual(sent[:3], ["time", "lock", "jobs"])


if __name__ == "__main__":
    unittest.main()
