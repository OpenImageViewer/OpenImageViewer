"""Check viewer startup with no input, an image, and a folder; requires a graphical session.

Usage: python Tests/Scripts/check_viewer_startup.py path/to/bin/OIViewer
This checks process survival. Verify the displayed content separately.
"""

import argparse
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--renderer", choices=("D3D11", "Vulkan", "GL"), help="explicit renderer also prevents file forwarding to an existing viewer")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    root = Path(__file__).resolve().parents[2]
    inputs = (
        ("no input", None),
        ("image", root / "External/ImageCodec/Example/cat.jpg"),
        ("folder", root / "External/ImageCodec/External/FreeImageRe/TestAPI"),
    )
    failures = 0
    for label, path in inputs:
        if path is not None:
            path = path.resolve(strict=True)
        command = [str(executable)] + (["--renderer", args.renderer] if args.renderer else []) + ([str(path)] if path is not None else [])
        process = subprocess.Popen(
            command, cwd=executable.parent, stdout=subprocess.PIPE, stderr=subprocess.STDOUT
        )
        try:
            try:
                output, _ = process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                print(f"PASS {label}: remained running for 5 seconds", flush=True)
            else:
                failures += 1
                print(f"FAIL {label}: exited early with code {process.returncode}", flush=True)
                if output:
                    print(output.decode("utf-8", errors="replace"), flush=True)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.communicate()
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())
