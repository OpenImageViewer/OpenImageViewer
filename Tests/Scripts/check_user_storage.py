"""Verify cross-process storage leases and version cleanup in isolated user directories."""
import argparse
import json
import os
import sys
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    executable = parser.parse_args().executable.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="OIViewer-storage-") as temporary:
        root = Path(temporary)
        processes = []
        def hold(version, name):
            ready = root / name
            process = subprocess.Popen([str(executable), str(root), version, str(ready)],
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            processes.append(process)
            deadline = time.monotonic() + 15
            while not ready.exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(.02)
            if not ready.exists():
                raise RuntimeError("Storage process did not acquire its lease")
            return process
        def run(version, success):
            result = subprocess.run([str(executable), str(root), version], capture_output=True, timeout=15)
            if (result.returncode == 0) != success:
                raise RuntimeError(result.stderr.decode(errors="replace"))
            if not success and b"Another OIViewer version" not in result.stderr:
                raise RuntimeError("Storage conflict did not explain how to recover")
        try:
            first = hold("1.2.3.4", "first-ready")
            second = hold("1.2.3.4", "second-ready")
            generated = root / "Cache/Generated"
            generated.mkdir(exist_ok=True)
            cached = generated / "shader.bin"
            cached.write_bytes(b"compiled shader")
            (root / "Config").mkdir(exist_ok=True)
            preferences = root / "Config/association-prompts.json"
            preferences.write_bytes(b"preferences")
            (root / "State").mkdir(exist_ok=True)
            log = root / "State/OIViewer.log"
            log.write_bytes(b"old log")
            run("1.2.3.4", True)
            run("1.2.3.5", False)
            assert cached.exists()
            first.kill()
            first.communicate(timeout=5)
            run("1.2.3.5", False)
            second.communicate(input=b"", timeout=5)
            run("1.2.3.5", True)
            assert json.loads((root / "Cache/cache-state.json").read_text())["version"] == "1.2.3.5"
            assert not cached.exists()
            run("1.2.3.3", True)
            assert json.loads((root / "Cache/cache-state.json").read_text())["version"] == "1.2.3.3"
            assert preferences.read_bytes() == b"preferences"
            assert log.read_bytes() == b"old log"
            def redirect_to_state(link):
                if sys.platform == "win32":
                    script = root / "junction.ps1"
                    script.write_bytes(("param([string]$LinkPath, [string]$TargetPath)\r\n"
                                        "$ErrorActionPreference = 'Stop'\r\n"
                                        "New-Item -ItemType Junction -Path $LinkPath -Value $TargetPath | Out-Null\r\n").encode())
                    subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
                                    str(script), str(link), str(root / "State")], check=True, capture_output=True, timeout=15)
                else:
                    link.symlink_to(root / "State", target_is_directory=True)
            link = root / "Cache/Generated"
            redirect_to_state(link)
            try:
                result = subprocess.run([str(executable), str(root), "1.2.3.6"], capture_output=True, timeout=15)
                assert result.returncode != 0 and b"symlink or junction" in result.stderr
                assert log.read_bytes() == b"old log"
            finally:
                if sys.platform == "win32":
                    os.rmdir(link)
                else:
                    link.unlink()
            link.mkdir()
            redirect_to_state(link / "nested-link")
            run("1.2.3.7", True)
            assert not link.exists()
            assert log.read_bytes() == b"old log"
            print("PASS: shared leases, version conflicts, termination recovery, upgrade/downgrade and redirected-cache protection")
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=5)


if __name__ == "__main__":
    main()
