"""Exercise the production callback factory in an isolated owner fixture with real LWS draining.

Run from an x64 Visual Studio development shell after building LWSLib:
    python Tests/Scripts/check_viewer_callback_lifetime.py build/windows-clang

This tests MakeSafeCallback's lifetime gate, not the complete ViewerApplication.
The factory is read from source, so the fixture does not keep a second implementation.
Use --baseline-ref <pre-fix-revision> to check that the former factory fails the same regression.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_directory", type=Path)
    parser.add_argument("--baseline-ref")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("This harness uses the Win32 LWS backend and requires an x64 Visual Studio development shell.")
    root = Path(__file__).resolve().parents[2]
    build = args.build_directory.resolve(strict=True)
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    compiler = re.search(r"^CMAKE_CXX_COMPILER:[^=]+=(.+)$", cache, re.MULTILINE).group(1).strip()
    debug = "CMAKE_BUILD_TYPE:STRING=Debug" in cache
    relative_source = "Clients/OIViewer/Source/ViewerApplicationUI.cpp"
    if args.baseline_ref:
        source = subprocess.check_output(
            ["git", "show", f"{args.baseline_ref}:{relative_source}"], cwd=root, text=True, encoding="utf-8"
        )
    else:
        source = (root / relative_source).read_text(encoding="utf-8")
    start = source.index("std::function<void()> ViewerApplication::MakeSafeCallback(")
    # The next method is the constructor. Keep the complete factory definition unchanged.
    end = source.index("ViewerApplication::ViewerApplication(", start)
    definition = source[start:end].rstrip()
    fixture = (root / "Tests/ViewerCallbacks/CallbackLifetime.cpp.in").read_text(encoding="utf-8")
    generated = fixture.replace("// @MAKE_SAFE_CALLBACK@", definition)
    output = build / "callback-lifetime-check" / ("baseline" if args.baseline_ref else "current")
    output.mkdir(parents=True, exist_ok=True)
    cpp = output / "CallbackLifetime.cpp"
    cpp.write_text(generated, encoding="utf-8", newline="\r\n")
    executable = output / "CallbackLifetime.exe"
    command = [
        compiler, "/nologo", "/std:c++latest", "/EHsc", "/MDd" if debug else "/MD", "/Od",
        "/DUNICODE", "/D_UNICODE", "/DNOMINMAX", "/DLWS_PLATFORM_WIN32", "/DLWS_HAS_WIN32_BACKEND",
        f"/I{root / 'External/LWS/src/LWS/include'}", f"/I{root / 'External/LLUtils/Include'}",
        str(cpp), f"/Fe:{executable}", f"/Fo:{output / 'CallbackLifetime.obj'}",
        "/link", str(build / "External/LWS/LWSLib.lib"),
        "ole32.lib", "shell32.lib", "user32.lib", "gdi32.lib", "comctl32.lib", "dbghelp.lib",
    ]
    subprocess.run(command, cwd=root, check=True)
    # Run shutdown independently so an early-invalidation failure cannot hide the destroyed-owner regression.
    results = [subprocess.run([str(executable), *scenario], cwd=output).returncode
               for scenario in ([], ["shutdown"])]
    return int(any(results))


if __name__ == "__main__":
    raise SystemExit(main())
