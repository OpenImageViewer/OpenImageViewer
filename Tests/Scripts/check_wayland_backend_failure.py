"""Verify viewer cleanup after losing a private Wayland compositor.

Requires Linux, Weston with its headless backend, and a Wayland OIViewer build.
Usage: python3 Tests/Scripts/check_wayland_backend_failure.py build/linux-clang/bin/OIViewer
The compositor is private to this test; the user's desktop is not stopped.
"""

import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time


def stop(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--log-dir", type=Path, default=Path("build/wayland-failure-smoke"))
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    logs = args.log_dir.resolve()
    logs.mkdir(parents=True, exist_ok=True)
    viewer_log = logs / "viewer.log"
    compositor = viewer = None
    with tempfile.TemporaryDirectory(prefix="lws-failure-") as runtime:
        env = dict(os.environ, XDG_RUNTIME_DIR=runtime, WAYLAND_DISPLAY="lws-failure", LIBGL_ALWAYS_SOFTWARE="1")
        try:
            compositor = subprocess.Popen(
                ["weston", "--backend=headless", "--renderer=pixman", "--socket=lws-failure", "--idle-time=0",
                 "--no-config", "--log=" + str(logs / "weston.log")],
                env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            deadline = time.monotonic() + 10
            socket = Path(runtime) / "lws-failure"
            while not socket.exists() and compositor.poll() is None and time.monotonic() < deadline:
                time.sleep(0.1)
            if not socket.exists():
                raise RuntimeError("private compositor failed to start")
            with viewer_log.open("wb") as output:
                viewer = subprocess.Popen([str(executable)], cwd=executable.parent, env=env,
                                          stdout=output, stderr=subprocess.STDOUT)
                time.sleep(5)
                if viewer.poll() is not None:
                    raise RuntimeError(f"viewer exited before failure injection: {viewer.returncode}")
                compositor.terminate()
                compositor.wait(timeout=5)
                code = viewer.wait(timeout=15)
            if code != 1:
                raise RuntimeError(f"expected reported failure exit 1, got {code}")
            if "LWS backend failure:" not in viewer_log.read_text(errors="replace"):
                raise RuntimeError("viewer did not report the backend diagnostic")
            print("PASS: compositor loss reports failure and cleans up without a crash")
            return 0
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            print(f"FAIL: {error}; logs: {logs}")
            return 1
        finally:
            stop(viewer)
            stop(compositor)


if __name__ == "__main__":
    raise SystemExit(main())
