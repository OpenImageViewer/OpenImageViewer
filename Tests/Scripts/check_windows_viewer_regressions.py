"""Exercise Windows viewer startup, repeated browsing, window transitions, and orderly shutdown.

Run with a graphical session:
  python Tests/Scripts/check_windows_viewer_regressions.py path/to/OIViewer.exe
Use --renderer D3D11, Vulkan, or GL to exercise one backend explicitly.
The script addresses only windows belonging to processes it starts.
"""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

WM_CLOSE, WM_KEYDOWN, WM_KEYUP = 0x10, 0x100, 0x101
VK_RIGHT = 0x27
SW_MAXIMIZE, SW_MINIMIZE, SW_RESTORE = 3, 6, 9
user = ctypes.WinDLL("user32", use_last_error=True)
callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
user.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
user.ShowWindowAsync.argtypes = [wintypes.HWND, ctypes.c_int]
user.IsIconic.argtypes = [wintypes.HWND]
user.IsZoomed.argtypes = [wintypes.HWND]
user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user.MapVirtualKeyW.argtypes = [wintypes.UINT, wintypes.UINT]
user.GetPropW.argtypes = [wintypes.HWND, wintypes.LPCWSTR]
user.GetPropW.restype = wintypes.HANDLE


def require_no_existing_tray_viewer():
    existing = []

    @callback_type
    def visit(window, _):
        if user.GetPropW(window, "isTrayWindow"):
            existing.append(window)
        return True

    user.EnumWindows(visit, 0)
    if existing:
        raise RuntimeError("Default startup would forward input to an existing viewer; close it or use --renderer")


def title(window):
    text = ctypes.create_unicode_buffer(2048)
    user.GetWindowTextW(window, text, len(text))
    return text.value


def find_window(process):
    windows = []

    @callback_type
    def visit(window, _):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == process.pid:
            name = ctypes.create_unicode_buffer(128)
            user.GetClassNameW(window, name, len(name))
            if name.value == "LWS_WINDOW_CLASS" and title(window):
                windows.append(window)
        return True

    user.EnumWindows(visit, 0)
    return windows[0] if windows else None


def wait_for(process, predicate, description, timeout=20):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            output, _ = process.communicate()
            raise RuntimeError(f"{description}: process exited {process.returncode}: "
                               + output.decode(errors="replace")[-2000:])
        result = predicate()
        if result:
            return result
        time.sleep(0.05)
    window = find_window(process)
    details = f"; window={rectangle(window)}, title={title(window)!r}" if window else "; no viewer window"
    raise RuntimeError(f"Timed out: {description}{details}")


def press(window, key, extended=False):
    scan = user.MapVirtualKeyW(key, 0)
    flags = 1 | (scan << 16) | (int(extended) << 24)
    for message, bits in ((WM_KEYDOWN, flags), (WM_KEYUP, flags | 0xC0000000)):
        if not user.PostMessageW(window, message, key, bits):
            raise ctypes.WinError(ctypes.get_last_error())


def rectangle(window):
    rect = wintypes.RECT()
    if not user.GetWindowRect(window, ctypes.byref(rect)):
        raise ctypes.WinError(ctypes.get_last_error())
    return rect.left, rect.top, rect.right, rect.bottom


def exercise(executable, renderer, label, path, expected_name=None, browse_names=()):
    # Explicit renderer selection starts a separate instance. Older/default startup can forward a path to a
    # tray viewer, so check before each launch to avoid changing an unrelated instance's displayed image.
    if not renderer:
        require_no_existing_tray_viewer()
    command = [str(executable)]
    if renderer:
        command += ["--renderer", renderer]
    if path is not None:
        command.append(str(path))
    process = subprocess.Popen(command, cwd=executable.parent,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        window = wait_for(process, lambda: find_window(process), f"{label} startup")
        if expected_name:
            wait_for(process, lambda: expected_name in title(window), f"{label} initial image")
        # Each navigation starts after the previous asynchronous completion reached the title.
        # This catches a drain flag that is never reset after the first batch.
        for name in browse_names:
            press(window, VK_RIGHT, extended=True)
            wait_for(process, lambda: name in title(window), f"browse to {name}")

        normal = rectangle(window)
        user.ShowWindowAsync(window, SW_MINIMIZE)
        wait_for(process, lambda: user.IsIconic(window), f"{label} minimize")
        user.ShowWindowAsync(window, SW_RESTORE)
        wait_for(process, lambda: not user.IsIconic(window) and rectangle(window) == normal,
                 f"{label} restore to {normal}")
        user.ShowWindowAsync(window, SW_MAXIMIZE)
        wait_for(process, lambda: user.IsZoomed(window), f"{label} maximize")
        user.ShowWindowAsync(window, SW_RESTORE)
        wait_for(process, lambda: rectangle(window) == normal, f"{label} restore placement to {normal}")
        # Exercise the migrated fullscreen and window-size commands through real key messages.
        press(window, ord("4"))
        wait_for(process, lambda: rectangle(window) != normal, f"{label} fullscreen")
        fullscreen = rectangle(window)
        press(window, ord("2"))
        wait_for(process, lambda: rectangle(window) != fullscreen and not user.IsZoomed(window),
                 f"{label} windowed sizing")
        if not user.PostMessageW(window, WM_CLOSE, 0, 0):
            raise ctypes.WinError(ctypes.get_last_error())
        output, _ = process.communicate(timeout=20)
        if process.returncode != 0:
            raise RuntimeError(f"{label} close exited {process.returncode}: "
                               + output.decode(errors="replace")[-2000:])
        print(f"PASS {renderer or 'default'} {label}: startup, transitions, orderly close", flush=True)
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--renderer")
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    root = Path(__file__).resolve().parents[2]
    fixture = (root / "External/ImageCodec/Example/cat.jpg").resolve(strict=True)
    if args.renderer and args.renderer.casefold() in ("d3d11", "vulkan"):
        # A valid but absent adapter fails after window creation, while initial image decoding may be active.
        failed = subprocess.run([str(executable), "--renderer", args.renderer, "--adapter-index", str(2**31 - 1),
                                 str(fixture)], cwd=executable.parent, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=20)
        if failed.returncode != 1:
            raise RuntimeError(f"Renderer initialization failure exited {failed.returncode}: "
                               + failed.stdout.decode(errors="replace")[-2000:])
        print(f"PASS {args.renderer}: failed initialization exits cleanly", flush=True)
    exercise(executable, args.renderer, "empty", None)
    exercise(executable, args.renderer, "image", fixture, "cat.jpg")
    corpus = (root / "External/ImageCodec/External/FreeImageRe/TestAPI").resolve(strict=True)
    exercise(executable, args.renderer, "folder", corpus)
    with tempfile.TemporaryDirectory(prefix="oiv-browse-regression-") as temporary:
        folder = Path(temporary)
        names = [f"{index:02d}-browse.jpg" for index in range(4)]
        for name in names:
            shutil.copyfile(fixture, folder / name)
        exercise(executable, args.renderer, "successive completion batches", folder / names[0],
                 names[0], names[1:])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
