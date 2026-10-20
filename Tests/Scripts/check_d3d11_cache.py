"""Verify D3D11 Release shader cache creation, reuse, and recovery using the built viewer.

Run on Windows with a Release viewer. Only generated D3D11 cache files are modified.
"""
import argparse
import ctypes
from ctypes import wintypes
import os
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    executable = parser.parse_args().executable.resolve(strict=True)
    if sys.platform != "win32":
        raise SystemExit("D3D11 cache checks require Windows")
    from check_windows_viewer_regressions import exercise, require_no_existing_tray_viewer
    require_no_existing_tray_viewer()
    generated = Path(os.environ["APPDATA"]) / "OIViewer/Cache/Generated"
    cache = generated / "D3D11/Release"
    if not cache.resolve().is_relative_to(generated.resolve()):
        raise RuntimeError("The cache test cannot clean a redirected renderer cache directory")
    names = ("QuadVP.bin", "QuadFP.bin", "QuadSelectionFP.bin", "QuadSimpleFP.bin")
    for name in names:
        (cache / name).unlink(missing_ok=True)
    exercise(executable, "D3D11", "cold shader cache", None)
    files = sorted(cache.glob("*.bin"))
    if len(files) != 4 or any(not file.read_bytes().startswith(b"DXBC") for file in files):
        raise RuntimeError("Release startup did not produce four complete D3D11 shader caches")
    original = {file: (file.stat().st_mtime_ns, file.read_bytes()) for file in files}
    exercise(executable, "D3D11", "warm shader cache", None)
    if any((file.stat().st_mtime_ns, file.read_bytes()) != content for file, content in original.items()):
        raise RuntimeError("A valid warm shader cache was unexpectedly rewritten")
    victim = files[0]
    try:
        victim.write_bytes(b"invalid cached bytecode")
        exercise(executable, "D3D11", "corrupt shader cache", None)
        if victim.read_bytes() != original[victim][1]:
            raise RuntimeError("Corrupt shader bytecode was not rebuilt from source")
        victim.write_bytes(b"")
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p,
                                       wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        held = kernel.CreateFileW(str(victim), 0x80000000, 3, None, 3, 0x80, None)
        if held == ctypes.c_void_p(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            exercise(executable, "D3D11", "unwritable shader cache", None)
            if victim.read_bytes():
                raise RuntimeError("The test did not prevent cache replacement")
        finally:
            kernel.CloseHandle(held)
        exercise(executable, "D3D11", "empty shader cache", None)
        if victim.read_bytes() != original[victim][1]:
            raise RuntimeError("Empty shader bytecode was not rebuilt from source")
        print("PASS D3D11 Release: cache reuse, corrupt/empty recovery and nonfatal cache write failure")
    finally:
        victim.write_bytes(original[victim][1])



if __name__ == "__main__":
    main()
