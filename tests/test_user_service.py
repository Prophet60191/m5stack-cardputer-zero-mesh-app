# SPDX-License-Identifier: MIT
"""No systemd/radio calls: exercise service lifecycle, locking and fail-closed paths."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "supervisor"))
import user_service


def response(stdout="", code=0):
    return subprocess.CompletedProcess([], code, stdout, "")


class UserServiceTests(unittest.TestCase):
    def test_transient_service_keeps_privilege_and_cleanup_contract(self):
        args = user_service.start_command()
        self.assertEqual(args[0], "/usr/bin/systemd-run")
        self.assertIn("--user", args)
        self.assertIn("--collect", args)
        self.assertIn("--property=Type=exec", args)
        self.assertIn("--property=KillMode=control-group", args)
        self.assertIn("--property=NoNewPrivileges=yes", args)
        self.assertIn("--property=UMask=0077", args)
        self.assertIn("--property=Restart=on-failure", args)
        cleanup = next(a for a in args if a.startswith("--property=ExecStopPost="))
        self.assertIn(f"radio_guardian.py --runtime /run/user/{os.getuid()}/cz-mesh --cleanup", cleanup)
        self.assertNotIn("%t", cleanup)
        self.assertEqual(args[-1], "/usr/lib/cardputerzero-mesh/run-supervisor")
        self.assertFalse(any(a.startswith(("--system", "--uid", "--scope")) for a in args))

    def test_active_or_starting_service_is_never_restarted_by_another_launch(self):
        for state in ("active", "activating", "reloading"):
            with self.subTest(state=state), patch.object(user_service, "command", return_value=response(state)) as call:
                user_service.ensure()
                self.assertEqual(call.call_count, 1)

    def test_stopped_legacy_unit_is_reloaded_before_creating_transient_unit(self):
        with patch.object(user_service, "command", side_effect=[response("inactive"), response(), response(), response()]) as call:
            user_service.ensure()
        commands = [c.args for c in call.call_args_list]
        self.assertEqual(commands[1], (user_service.SYSTEMCTL, "--user", "reset-failed", user_service.UNIT))
        self.assertEqual(commands[2], (user_service.SYSTEMCTL, "--user", "daemon-reload"))
        self.assertEqual(list(commands[3]), user_service.start_command())

    def test_stopping_or_failed_manager_never_reports_a_new_start(self):
        with patch.object(user_service, "command", return_value=response("deactivating")) as call:
            with self.assertRaisesRegex(RuntimeError, "stopping"):
                user_service.ensure()
            self.assertEqual(call.call_count, 1)
        with patch.object(user_service, "command", side_effect=[response("failed"), response(), subprocess.CalledProcessError(1, "systemctl")]) as call:
            with self.assertRaises(subprocess.CalledProcessError):
                user_service.ensure()
            self.assertEqual(call.call_count, 3)

    def test_lock_is_private_and_rejects_symlinks_hardlinks_and_public_directory(self):
        with tempfile.TemporaryDirectory() as temp, patch.dict(os.environ, {"XDG_RUNTIME_DIR": temp}):
            path = Path(temp) / "cardputerzero-mesh-service.lock"
            fd = user_service.runtime_lock()
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            os.close(fd)
            target = Path(temp) / "target"
            path.rename(target)
            path.symlink_to(target)
            with self.assertRaises(OSError):
                user_service.runtime_lock()
            path.unlink(); os.link(target, path)
            with self.assertRaises(PermissionError):
                user_service.runtime_lock()
            path.unlink(); target.unlink()
            Path(temp).chmod(0o777)
            with self.assertRaises(PermissionError):
                user_service.runtime_lock()

    def test_concurrent_operation_times_out_without_replacing_lock(self):
        with tempfile.TemporaryDirectory() as temp, patch.dict(os.environ, {"XDG_RUNTIME_DIR": temp}):
            fd = user_service.runtime_lock()
            try:
                inode = os.fstat(fd).st_ino
                with patch.object(user_service.time, "monotonic", side_effect=[0, 11]):
                    with self.assertRaises(TimeoutError):
                        user_service.runtime_lock()
                self.assertEqual((Path(temp) / "cardputerzero-mesh-service.lock").stat().st_ino, inode)
            finally:
                os.close(fd)

    def invoke(self, action, responses):
        with tempfile.TemporaryDirectory() as temp, patch.dict(os.environ, {"XDG_RUNTIME_DIR": temp}), \
             patch.object(user_service.os, "geteuid", return_value=1000), \
             patch.object(sys, "argv", ["user_service.py", action]), \
             patch.object(user_service, "command", side_effect=responses) as call:
            result = user_service.main()
            return result, [c.args for c in call.call_args_list]

    def test_restart_waits_for_stop_before_start_and_stop_does_not_start(self):
        code, commands = self.invoke("restart", [response(), response("inactive"), response(), response(), response()])
        self.assertEqual(code, 0)
        self.assertEqual(commands[0], (user_service.SYSTEMCTL, "--user", "stop", user_service.UNIT))
        self.assertEqual(list(commands[-1]), user_service.start_command())
        code, commands = self.invoke("stop", [response()])
        self.assertEqual(code, 0)
        self.assertEqual(len(commands), 1)

    def test_failed_stop_never_proceeds_to_start_but_missing_unit_is_idempotent(self):
        code, commands = self.invoke("restart", [response(code=1), response("loaded")])
        self.assertEqual(code, 1)
        self.assertEqual(len(commands), 2)
        code, commands = self.invoke("stop", [response(code=1), response("not-found")])
        self.assertEqual(code, 0)

    def test_root_and_unknown_action_are_refused_before_any_command(self):
        for uid, action in ((0, "ensure"), (1000, "arbitrary-command")):
            with patch.object(user_service.os, "geteuid", return_value=uid), \
                 patch.object(sys, "argv", ["user_service.py", action]), \
                 patch.object(user_service, "command") as call:
                with self.assertRaises(SystemExit) as exit:
                    user_service.main()
                self.assertEqual(exit.exception.code, 2)
                call.assert_not_called()
