"""Smoke-test WLX in Total Commander or Double Commander with an isolated profile."""
import argparse
import re
import ctypes as c
from ctypes import wintypes as w
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

u = c.WinDLL("user32", use_last_error=True)
callback = c.WINFUNCTYPE(w.BOOL, w.HWND, w.LPARAM)
u.EnumWindows.argtypes = [callback, w.LPARAM]
u.EnumChildWindows.argtypes = [w.HWND, callback, w.LPARAM]
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]
u.GetClassNameW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.GetWindowTextW.argtypes = [w.HWND, w.LPWSTR, c.c_int]
u.PostMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.IsWindow.argtypes = [w.HWND]
u.SendMessageW.argtypes = [w.HWND, w.UINT, w.WPARAM, w.LPARAM]
u.GetMenu.argtypes = [w.HWND]
u.GetMenu.restype = w.HMENU
u.GetSubMenu.argtypes = [w.HMENU, c.c_int]
u.GetSubMenu.restype = w.HMENU
u.GetMenuItemCount.argtypes = [w.HMENU]
u.GetMenuItemID.argtypes = [w.HMENU, c.c_int]
u.GetMenuItemID.restype = w.UINT
u.GetMenuStringW.argtypes = [w.HMENU, w.UINT, w.LPWSTR, c.c_int, w.UINT]


def classname(hwnd):
    value = c.create_unicode_buffer(256)
    u.GetClassNameW(hwnd, value, len(value))
    return value.value


def title(hwnd):
    value = c.create_unicode_buffer(4096)
    u.GetWindowTextW(hwnd, value, len(value))
    return value.value


def menu_commands(menu):
    commands = {}
    for index in range(u.GetMenuItemCount(menu)):
        text = c.create_unicode_buffer(256)
        u.GetMenuStringW(menu, index, text, len(text), 0x400)
        caption = text.value.split("\t")[0].replace("&", "").lower()
        commands[caption] = u.GetMenuItemID(menu, index)
        submenu = u.GetSubMenu(menu, index)
        if submenu:
            commands.update(menu_commands(submenu))
    return commands


def windows(pid, parent=None):
    found = []

    @callback
    def visit(hwnd, _):
        owner = w.DWORD()
        u.GetWindowThreadProcessId(hwnd, c.byref(owner))
        if owner.value == pid:
            found.append(hwnd)
        return True

    if parent:
        u.EnumChildWindows(parent, visit, 0)
    else:
        u.EnumWindows(visit, 0)
    return found


def wait(process, predicate, description):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"Host exited {process.returncode} while waiting for {description}")
        value = predicate()
        if value:
            return value
        time.sleep(0.1)
    raise RuntimeError(f"Timed out waiting for {description}")


def exercise_doublecmd_navigation(process, viewer, composite, image):
    commands = menu_commands(u.GetMenu(viewer))

    def choose(command, filename):
        u.PostMessageW(viewer, 0x111, commands[command], 0)
        wait(process, lambda: filename in title(viewer) and filename in title(composite),
             f"host {command} to {filename}")
        if not u.IsWindow(composite):
            raise RuntimeError("Host navigation replaced the plugin instead of reusing ListLoadNext")

    choose("next", "oiv-next.jpg")
    choose("previous", image.name)
    # Local browsing does not change the host's list index. Host Next may therefore select the same
    # image, which looks unchanged even though ListLoadNext succeeds and advances the host state.
    u.PostMessageW(composite, 0x100, 0x27, 1 | (0x4D << 16) | (1 << 24))
    u.PostMessageW(composite, 0x101, 0x27, 0xC0000001 | (0x4D << 16) | (1 << 24))
    wait(process, lambda: "oiv-next.jpg" in title(composite), "plugin-local next image")
    if image.name not in title(viewer):
        raise RuntimeError("Independent plugin browsing unexpectedly changed the host's current file")
    choose("next", "oiv-next.jpg")
    choose("next", "oiv-third.jpg")
    choose("previous", "oiv-next.jpg")
    print("PASS doublecmd: host Next/Previous and independent plugin navigation", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", choices=["totalcmd", "doublecmd"])
    parser.add_argument("binary", type=Path)
    parser.add_argument("plugin", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    plugin = args.plugin.resolve(strict=True)
    binary = args.binary.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix=f"oiv-{args.host}-") as directory:
        directory = Path(directory)
        profile = directory / "config"
        profile.mkdir()
        # Exercise forwarding with a private keymap, preserving all installed user bindings.
        copied = directory / "plugin"
        shutil.copytree(plugin.parent, copied)
        plugin = copied / plugin.name
        keymap = copied / "Resources/Configuration/KeyBindings.json"
        bindings = keymap.read_text(encoding="utf-8-sig")
        bindings = re.sub(r'^.*\{\s*"[NP]"\s*:.*\}.*\n', '', bindings, flags=re.M)
        keymap.write_bytes(bindings.replace("\n", "\r\n").encode("utf-8"))
        images = directory / "images"
        images.mkdir()
        image = images / "oiv-integration.jpg"
        shutil.copyfile(root / "External/ImageCodec/Example/cat.jpg", image)
        for filename in ("oiv-next.jpg", "oiv-third.jpg"):
            shutil.copyfile(image, image.with_name(filename))
        if args.host == "totalcmd":
            # TC stores the nominal .wlx name and substitutes .wlx64 in its 64-bit process.
            ini = profile / "wincmd.ini"
            text = (f'[Configuration]\nFirstTime=0\n[ListerPlugins]\n0={plugin.with_suffix(".wlx")}\n'
                    '0_detect=MULTIMEDIA & (EXT="JPG")\n[ListerPlugins64]\n0=1\n')
            ini.write_bytes(text.replace("\n", "\r\n").encode("mbcs"))
            command = [str(binary), "/N", f"/I={ini}", f"/F={profile / 'wcx.ini'}",
                       f"/L={images}"]
        else:
            config = ET.Element("doublecmd")
            viewer = ET.SubElement(config, "Viewer")
            ET.SubElement(viewer, "ViewerMode").text = "0"
            plugins = ET.SubElement(config, "Plugins", Version="3")
            entry = ET.SubElement(ET.SubElement(plugins, "WlxPlugins"), "WlxPlugin", Enabled="True")
            ET.SubElement(entry, "Name").text = "OIViewer"
            ET.SubElement(entry, "Path").text = str(plugin)
            ET.SubElement(entry, "DetectString").text = 'MULTIMEDIA & (EXT="JPG")'
            ET.indent(config)
            (profile / "doublecmd.xml").write_bytes(
                ET.tostring(config, encoding="utf-8", xml_declaration=True).replace(b"\n", b"\r\n"))
            command = [str(binary), f"--config-dir={profile}",
                       f"--servername=oiv-test-{directory.name}", "--no-splash", f"-L{image}"]
        process = subprocess.Popen(command)
        try:
            main_window = None
            if args.host == "doublecmd":
                main_window = wait(process, lambda: next(
                    (h for h in windows(process.pid) if classname(h) == "TTOTAL_CMD" or classname(h).startswith("TfrmMain")), None),
                    "main window")
                time.sleep(1)
                u.PostMessageW(main_window, 0x100, 0x72, 1)
                u.PostMessageW(main_window, 0x101, 0x72, 0xC0000001)
            if args.host == "totalcmd":
                main_window = wait(process, lambda: next(
                    (h for h in windows(process.pid) if classname(h) == "TTOTAL_CMD"), None),
                    "main window")
                def dismiss_prompts():
                    dialogs = windows(process.pid)
                    for dialog in dialogs:
                        children = windows(process.pid, dialog)
                        if classname(dialog) == "TNASTYNAGSCREEN":
                            number = next((title(h) for h in children if title(h) in ("1", "2", "3")
                                           and classname(h) != "Button"), None)
                            for h in children:
                                if number and title(h) == "&" + number:
                                    u.SendMessageW(h, 0xF5, 0, 0)
                            return False
                        if classname(dialog) == "TICONLIBCHANGEDLG":
                            for h in children:
                                if title(h).replace("&", "") == "Cancel":
                                    u.SendMessageW(h, 0xF5, 0, 0)
                            return False
                    return True
                time.sleep(1)
                wait(process, dismiss_prompts, "startup prompts")
                u.SendMessageW(main_window, 1075, 523, 0)
                u.SendMessageW(main_window, 1075, 903, 0)
            def find_viewer():
                for window in windows(process.pid):
                    if sum(classname(h) == "LWS_WINDOW_CLASS" for h in windows(process.pid, window)) >= 3:
                        return window
                return None

            viewer = wait(process, find_viewer, "OIV lister window hierarchy")
            children = [h for h in windows(process.pid, viewer) if classname(h) == "LWS_WINDOW_CLASS"]
            if len(children) != 3:
                raise RuntimeError(f"Expected container, composite and canvas, found {len(children)}")
            time.sleep(1)
            if process.poll() is not None:
                raise RuntimeError("Host exited while displaying the image")
            print(f"PASS {args.host}: plugin loaded with {len(children)} LWS windows", flush=True)
            # N/P are intentionally absent from this private plugin keymap.
            for key, filename in ((ord("N"), "oiv-next.jpg"), (ord("P"), image.name)):
                u.PostMessageW(children[1], 0x100, key, 1)
                u.PostMessageW(children[1], 0x101, key, 0xC0000001)
                wait(process, lambda: filename in title(viewer) and filename in title(children[1]),
                     f"forwarded {chr(key)} to {filename}")
            print(f"PASS {args.host}: unbound N/P forwarded to host navigation", flush=True)
            if args.host == "doublecmd":
                exercise_doublecmd_navigation(process, viewer, children[1], image)
            u.PostMessageW(children[1], 0x100, 0x1B, 1)
            u.PostMessageW(children[1], 0x101, 0x1B, 0xC0000001)
            if main_window:
                wait(process, lambda: not u.IsWindow(viewer), "viewer cleanup")
                u.PostMessageW(main_window, 0x10, 0, 0)
            code = process.wait(timeout=15)
            if code != 0:
                raise RuntimeError(f"Host exit code {code}")
            print(f"PASS {args.host}: Escape and orderly shutdown", flush=True)
        finally:
            if process.poll() is None:
                for hwnd in windows(process.pid):
                    u.PostMessageW(hwnd, 0x10, 0, 0)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    process.wait(timeout=5)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
