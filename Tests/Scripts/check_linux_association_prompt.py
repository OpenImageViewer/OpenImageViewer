"""Check Linux prompt responsiveness, cross-process ownership, deferral and shutdown on a private compositor."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("probe", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    probe = args.probe.resolve(strict=True)
    processes = []
    compositor = None
    with tempfile.TemporaryDirectory(prefix="oiv-prompt-") as temporary:
        root = Path(temporary)
        env = dict(os.environ, XDG_RUNTIME_DIR=str(root), WAYLAND_DISPLAY="oiv-prompt",
                   GDK_BACKEND="wayland", LIBGL_ALWAYS_SOFTWARE="1")
        for kind in ("CONFIG", "STATE", "CACHE", "DATA"):
            env[f"XDG_{kind}_HOME"] = str(root / kind.lower())
        def wait(predicate, description):
            deadline = time.monotonic() + 20
            while not predicate() and time.monotonic() < deadline:
                time.sleep(.05)
            if not predicate():
                raise RuntimeError("Timed out: " + description)
        def marker(process, suffix):
            return root / (str(process.pid) + suffix)
        def launch(path=None):
            output = (root / f"viewer-{len(processes)}.log").open("wb")
            try:
                process = subprocess.Popen([str(executable), *([str(path)] if path else [])],
                    env=dict(env, LD_PRELOAD=str(probe), OIV_TEST_ASSOCIATION_DIR=str(root)),
                    stdout=output, stderr=subprocess.STDOUT)
            finally:
                output.close()
            processes.append(process)
            return process
        try:
            compositor = subprocess.Popen(["weston", "--backend=headless", "--renderer=pixman",
                "--socket=oiv-prompt", "--idle-time=0", "--no-config", "--log=" + str(root / "weston.log")],
                env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            wait(lambda: (root / "oiv-prompt").exists(), "compositor startup")
            first = launch()
            wait(lambda: marker(first, ".responsive").exists(), "LWS dispatch while prompting")
            second = launch()
            time.sleep(2)
            assert second.poll() is None and not marker(second, ".prompt").exists(), "Concurrent prompt"
            marker(first, ".cancel").touch()
            wait(lambda: marker(first, ".closed").exists(), "prompt cancellation")
            assert not (root / "config/OIViewer/association-prompts.json").exists(), "Deferral persisted suppression"
            source = Path(__file__).resolve().parents[2]
            for path in (source / "External/ImageCodec/Example/cat.jpg",
                         source / "External/ImageCodec/External/FreeImageRe/TestAPI"):
                viewer = launch(path)
                wait(lambda: marker(viewer, ".responsive").exists(), "image/folder startup while prompting")
                marker(viewer, ".cancel").touch()
                wait(lambda: marker(viewer, ".closed").exists(), "image/folder prompt cancellation")
                assert viewer.poll() is None
            pending = launch()
            wait(lambda: marker(pending, ".responsive").exists(), "prompt after deferral")
            for viewer in processes:
                marker(viewer, ".close-viewer").touch()
            for viewer in processes:
                assert viewer.wait(timeout=15) == 0, "Unexpected exit on native close"
            assert marker(pending, ".closed").exists(), "Shutdown left a pending dialog alive"
            print("PASS: responsive prompts, cross-process ownership, deferral, image/folder startup and pending-dialog shutdown")
        except Exception:
            for log in root.glob("*.log"):
                print(log.name, log.read_text(errors="replace")[-2500:])
            raise
        finally:
            for process in [*processes, compositor]:
                if process is not None and process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)


if __name__ == "__main__":
    main()
