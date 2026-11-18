"""Tests for sync.py's pure parts: python3 -m unittest discover -s host -p 'test_*.py' (from the repository)."""
import importlib.util
import json
import subprocess
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


LOCAL_ID = ".claude-work/0b1c2d3e-aaaa-bbbb-cccc-1234567890ab/live"
HOST_ID = "podman:web:.claude/0b1c2d3e-aaaa-bbbb-cccc-1234567890ab/job"
BAD_IDS = ["a;b/c/live", ".claude-work/$(id)/live", ".claude-work/`id`/live", ".claude-work/a b/live",
           '.claude-work/"x/live', ".claude-work/x'/live", ".claude-work/x\n/live", "../x/y/live", "-x/y/live",
           "x" * 301, "lab:../a/b/live", "", "a|b/c/live", "a&b/c/live", "a>b/c/live", "a\\b/c/live",
           ".claude-work/Grüße/live",
           # (glob characters: Talpy Den would take the id's middle as a pattern for a transcript's path)
           ".claude-work/*/live", ".claude-work/?/live", ".claude-work/[ab]/live", ".claude-work/a{b,c}/live",
           ".claude-work/~x/live", "lab*:.claude/a/live", "only/two"]


class SafetyFixTests(unittest.TestCase):
    MANAGER = ("WAYLAND_DISPLAY=wayland-0\nDISPLAY=:0\nXDG_CURRENT_DESKTOP=KDE\nLANG=de_DE.UTF-8\n"
               "XAUTHORITY=/run/user/1000/xauth_abc\nWEIRD=$'a\\nb'\n")

    def test_desktop_env_from_the_user_manager(self):
        run = lambda argv, **k: types.SimpleNamespace(returncode=0, stdout=self.MANAGER)  # noqa: E731
        env = sync.desktop_env({"PATH": "/usr/bin", "HOME": "/h", "DISPLAY": ":9"}, run)
        self.assertEqual((env["WAYLAND_DISPLAY"], env["DISPLAY"], env["XDG_CURRENT_DESKTOP"]), ("wayland-0", ":0", "KDE"))
        self.assertEqual((env["PATH"], env["HOME"]), ("/usr/bin", "/h"))
        self.assertNotIn("LANG", env)   # (only what a window needs)
        self.assertNotIn("WEIRD", env)

    def test_desktop_env_without_a_manager(self):
        def fail(argv, **k):
            raise FileNotFoundError(argv[0])
        self.assertEqual(sync.desktop_env({"PATH": "/usr/bin"}, fail), {"PATH": "/usr/bin"})

    def test_opener_runs_jump_with_the_desktop_env(self):
        seen = {}

        def run(argv, **k):
            seen.update(k.get("env") or {})
            return types.SimpleNamespace(returncode=0, stdout='{"ok": true, "message": ""}', stderr="")
        opener = sync.Opener(run=run, environment=lambda: {"WAYLAND_DISPLAY": "wayland-0", "PATH": "/usr/bin"})
        opener.start(LOCAL_ID, Path("/c"))
        opener.thread.join(5)
        self.assertEqual(seen.get("WAYLAND_DISPLAY"), "wayland-0")

    def test_fit_rows_drops_the_oldest_started_first(self):
        many = [job(id=f".claude/{i:032x}/live", name="n" * 40, startedAt=1794140000000 + i * 1000)
                for i in range(200)]
        many.append(job(id=".claude/waits/live", state="waiting", startedAt=1))
        rows = sync.jobs_rows({"jobs": many})
        self.assertIn(".claude/waits/live", [r["id"] for r in rows])  # (the one waiting first)
        kept = sorted(r["started"] for r in rows if r["state"] == "working")
        self.assertEqual(kept[-1], 1794140000 + 199)  # the newest started is kept...
        self.assertGreater(kept[0], 1794140000 + 100)  # ...and the oldest ones went

    def test_open_log_line(self):
        self.assertEqual(sync.open_log(LOCAL_ID, True, "Switched to it"), f"open {LOCAL_ID}: ok Switched to it")
        line = sync.open_log("x\nforged: line\x1b[2J", False, "That chat isn't listed any more")
        self.assertEqual(line, "open (not a chat id): failed That chat isn't listed any more")


class FakeOpener:
    def __init__(self, free=True):
        self.free, self.started = free, []

    def start(self, job_id, code):
        if self.free:
            self.started.append((job_id, code))
        return self.free


class OpenTests(unittest.TestCase):
    def test_board_request(self):
        self.assertEqual(sync.board_request('{"type":"open","id":"%s"}' % LOCAL_ID), LOCAL_ID)
        for line in ('{"type":"ping"}', "not json", '["open"]', '{"type":"open","id":5}', '{"type":"open"}',
                     '{"type":"open","id":"%s"}' % ("x" * 1010), "PC back", "Setup done"):
            with self.subTest(line=line[:40]):
                self.assertIsNone(sync.board_request(line))

    def test_id_ok(self):
        for bad in BAD_IDS:
            with self.subTest(job_id=bad[:40]):
                self.assertFalse(sync.id_ok(bad))
        self.assertTrue(sync.id_ok(LOCAL_ID))
        self.assertTrue(sync.id_ok(HOST_ID))

    def test_jump_command_is_a_list(self):
        cmd = sync.jump_command(Path("/opt/td/code"), LOCAL_ID)
        self.assertEqual(cmd, ["python3", "-I", "/opt/td/code/jump.py", "--job", LOCAL_ID])

    def test_code_folder(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            home = Path(tmp)
            self.assertIsNone(sync.code_folder({}, home))
            for version in ("1.9.0", "1.21.0", "1.20.0"):
                code = home / f".vscode/extensions/wzaielamri.talpy-den-{version}/vendor/code"
                code.mkdir(parents=True)
                (code / "jump.py").write_text("")
            self.assertEqual(sync.code_folder({}, home).parent.parent.name, "wzaielamri.talpy-den-1.21.0")
            plasma = home / ".local/share/plasma/plasmoids/org.wzaielamri.talpyden/contents/code"
            plasma.mkdir(parents=True)
            (plasma / "jump.py").write_text("")
            self.assertEqual(sync.code_folder({}, home), plasma)
            mine = home / "td/code"
            mine.mkdir(parents=True)
            (mine / "jump.py").write_text("")
            self.assertEqual(sync.code_folder({"DESK_TALPY_CODE": str(mine)}, home), mine)
            self.assertIsNone(sync.code_folder({"DESK_TALPY_CODE": str(home / "nothing")}, home))

    def test_open_answers(self):
        code, listed = Path("/opt/td/code"), {LOCAL_ID, HOST_ID}
        opener = FakeOpener()
        self.assertEqual(sync.handle_open(".claude-work/other/live", listed, False, code, opener),
                         (False, "That chat isn't listed any more"))
        for bad in BAD_IDS:
            self.assertEqual(sync.handle_open(bad, listed | {bad}, False, code, opener)[0], False)
        self.assertEqual(sync.handle_open(LOCAL_ID, listed, True, code, opener), (False, "The PC is locked"))
        self.assertEqual(sync.handle_open(LOCAL_ID, listed, False, None, opener), (False, "Talpy Den not found"))
        self.assertEqual(opener.started, [])
        self.assertIsNone(sync.handle_open(HOST_ID, listed, False, code, opener))
        self.assertEqual(opener.started, [(HOST_ID, code)])
        self.assertEqual(sync.handle_open(LOCAL_ID, listed, False, code, FakeOpener(free=False)),
                         (False, "Still opening the last one"))

    def opener_answer(self, run):
        opener = sync.Opener(run=run)
        self.assertTrue(opener.start(LOCAL_ID, Path("/opt/td/code")))
        opener.thread.join(5)
        return opener.results.get_nowait()

    def test_opener_answers(self):
        def done(code, out="", err=""):
            return lambda *a, **k: types.SimpleNamespace(returncode=code, stdout=out, stderr=err)

        self.assertEqual(self.opener_answer(done(0, '{"ok": true, "message": "Switched to it"}\n')),
                         (LOCAL_ID, True, "Switched to it"))
        job_id, ok, message = self.opener_answer(done(0, json.dumps({"ok": False, "message": "x" * 200 + "é"})))
        self.assertFalse(ok)
        self.assertTrue(message.isascii() and len(message) == 80)
        self.assertEqual(self.opener_answer(done(1, err="usage: jump.py [--stop ...")),
                         (LOCAL_ID, False, "Talpy Den is too old for this"))

        def slow(*a, **k):
            raise subprocess.TimeoutExpired(a[0], k.get("timeout"))
        self.assertEqual(self.opener_answer(slow), (LOCAL_ID, False, "Talpy Den took too long"))
        self.assertEqual(sync.JUMP_TIMEOUT, 15)

    def test_opener_one_at_a_time(self):
        import threading
        go = threading.Event()

        def wait(*a, **k):
            go.wait(5)
            return types.SimpleNamespace(returncode=0, stdout='{"ok": true, "message": ""}', stderr="")
        opener = sync.Opener(run=wait)
        self.assertTrue(opener.start(LOCAL_ID, Path("/c")))
        self.assertFalse(opener.start(HOST_ID, Path("/c")))
        go.set()
        opener.thread.join(5)
        self.assertTrue(opener.start(HOST_ID, Path("/c")))
        opener.thread.join(5)


if __name__ == "__main__":
    unittest.main()
