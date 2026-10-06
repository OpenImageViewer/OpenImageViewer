"""Run fatal scenarios in children and verify diagnostics, including dialogs when enabled.

Usage: python Tests/Scripts/check_exception_reporting.py path/to/tests_exception_process
Windows needs an interactive desktop. Linux defaults to testing stderr without a display.
For Linux dialog coverage, pass --linux-dialog-probe path/to/libtests_exception_dialog_probe.so
with a working GTK display (a private headless Weston compositor is sufficient).
Allocation sweeps cover failures after construction has begun, not only the first allocation.
"""
import argparse
import os
import re
from pathlib import Path
import subprocess
import tempfile
import time


def windows_dialogs(process_id):
    import ctypes
    from ctypes import wintypes

    user32 = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user32.EnumChildWindows.argtypes = [wintypes.HWND, callback_type, wintypes.LPARAM]
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
    user32.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user32.SendMessageTimeoutW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM,
        wintypes.LPARAM, wintypes.UINT, wintypes.UINT, ctypes.POINTER(ctypes.c_size_t)]

    def title(window):
        text = ctypes.create_unicode_buffer(user32.GetWindowTextLengthW(window) + 1)
        user32.GetWindowTextW(window, text, len(text))
        return text.value

    found = []

    @callback_type
    def visit(window, _):
        pid = wintypes.DWORD()
        user32.GetWindowThreadProcessId(window, ctypes.byref(pid))
        if pid.value == process_id and title(window) == "OIViewer - Unhandled exception":
            parts = []

            @callback_type
            def child(control, _):
                # Static dialog controls require WM_GETTEXT across process boundaries.
                text = ctypes.create_unicode_buffer(16385)
                result = ctypes.c_size_t()
                user32.SendMessageTimeoutW(control, 0x000D, len(text), ctypes.addressof(text),
                                          2, 1000, ctypes.byref(result))
                parts.append(text.value)
                return True

            user32.EnumChildWindows(window, child, 0)
            found.append((int(window), "\n".join(parts)))
            if any(len(part) > 5 for part in parts):
                user32.PostMessageW(window, 0x0010, 0, 0)  # WM_CLOSE; button IDs vary by Windows version.
        return True

    user32.EnumWindows(visit, 0)
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--linux-dialog-probe", type=Path)
    parser.add_argument("--linux-close-control", choices=("button", "titlebar"), default="button")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    probe = args.linux_dialog_probe.resolve(strict=True) if args.linux_dialog_probe else None
    if probe and os.name == "nt":
        parser.error("--linux-dialog-probe is Linux-only")
    scenarios = {
        "standard": "standard failure",
        "long-unicode": "long report begin",
        "unknown": "Unknown/non-standard",
        "system": "system failure",
        "llutils": "original image failure",
        "worker": "worker failure",
        "worker-llutils": "worker LLUtils failure",
        "thread-unregistered": None,
        "terminate": "without an active exception",
        "noexcept": "noexcept failure",
        "allocation": "Diagnostics are unavailable",
        "duplicate": "reporter failure",
        "concurrent": "reporter failure",
        "default-event-allocation": None,
        "what-allocation": None,
        "handled-allocation": None,
        "handled-allocation-sweep": None,
        "format-allocation-sweep": None,
        "utf8-validation-allocation": None,
        "snapshot-allocation": None,
        "optional-allocation": None,
    }
    if os.name == "nt":
        scenarios.update({"native": "Access violation", "native-short-record": "In-page error", "handled-native": None,
                          "abort-policy": "abort policy check", "cpp-then-native": "reported before native failure"})
    failures = 0
    for scenario, expected in scenarios.items():
        dialogs = {}
        with tempfile.TemporaryDirectory(prefix="oiv-exception-dialog-") as dialog_dir, tempfile.TemporaryFile() as output:
            environment = os.environ.copy()
            emergency_path = Path(dialog_dir) / "emergency.log"
            if scenario in ("native", "native-short-record", "long-unicode"):
                environment["OIV_TEST_EMERGENCY_LOG"] = str(emergency_path)
            if os.name != "nt":
                if probe:
                    environment["LD_PRELOAD"] = str(probe)
                    environment["OIV_TEST_DIALOG_DIR"] = dialog_dir
                    environment["OIV_TEST_DIALOG_CLOSE"] = args.linux_close_control
                else:
                    environment["GDK_BACKEND"] = "x11"
                    environment.pop("DISPLAY", None)
            process = subprocess.Popen([str(executable), scenario], cwd=executable.parent,
                                       stdout=subprocess.DEVNULL, stderr=output, env=environment,
                                       start_new_session=os.name != "nt")
            deadline = time.monotonic() + 30
            while process.poll() is None and time.monotonic() < deadline:
                if os.name == "nt":
                    for handle, contents in windows_dialogs(process.pid):
                        # A message box can be enumerated while its controls are still being created.
                        if len(contents) > len(dialogs.get(handle, "")):
                            dialogs[handle] = contents
                time.sleep(0.03)
            timed_out = process.poll() is None
            if timed_out:
                if os.name == "nt":
                    process.kill()
                else:
                    import signal
                    os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            output.seek(0)
            text = output.read().decode("utf-8", errors="replace")
            emergency_text = emergency_path.read_text(encoding="utf-8") if emergency_path.exists() else ""
            if probe:
                dialogs = {path.stem: path.read_text(encoding="utf-8") for path in Path(dialog_dir).glob("*.txt")}
                closed_dialogs = {path.stem for path in Path(dialog_dir).glob("*.closed")}
        if expected is None:
            passed = not timed_out and process.returncode == 0 and not dialogs
        else:
            # This CRT/compiler can invoke terminate before exposing the noexcept cause.
            alternatives = (expected, "without an active exception") if scenario == "noexcept" else (expected,)
            exit_matches = process.returncode == 0 if scenario == "abort-policy" else process.returncode != 0
            passed = not timed_out and exit_matches and any(item in text for item in alternatives)
            if os.name == "nt" or probe:
                passed = passed and len(dialogs) == 1 and any(
                    item in next(iter(dialogs.values())) for item in alternatives)
                if probe:
                    passed = passed and next(iter(dialogs.values()), "").startswith("OIViewer - Unhandled exception\n")
                    passed = passed and set(dialogs) == closed_dialogs
            if scenario in ("native", "native-short-record", "long-unicode"):
                passed = passed and expected in emergency_text
            if scenario == "long-unicode":
                reports = (text, emergency_text, *dialogs.values())
                passed = passed and all("🌍" in report and "long report end" in report and
                                        report.count(expected) == 1 for report in reports)
            if scenario in ("duplicate", "concurrent", "cpp-then-native"):
                passed = passed and text.count(expected) == 1
            if scenario in ("llutils", "worker-llutils"):
                expected_roles = ["main", "main"] if scenario == "llutils" else ["worker"]
                for report in (text, *dialogs.values()):
                    threads = re.findall(r"Thread: ([1-9][0-9]*) \((main|worker|unknown)\)", report)
                    passed = passed and [role for _, role in threads] == expected_roles
                    if scenario == "llutils":
                        passed = passed and len({thread_id for thread_id, _ in threads}) == 1
        print(f"{'PASS' if passed else 'FAIL'} {scenario}: exit={process.returncode}, dialogs={len(dialogs)}", flush=True)
        if not passed:
            failures += 1
            print(text[:2000], dialogs, flush=True)
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
