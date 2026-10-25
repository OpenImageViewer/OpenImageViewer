# OIViewer WLX plugin

OIViewer runs inside Total Commander and Double Commander on Windows x64. The plugin retains the
canvas, sidebar, folder navigation, mouse gestures, image commands, and fullscreen.

## Build and install

Use an x64 Visual Studio developer shell with CMake, Ninja, Git for Windows, and clang-cl on PATH:

    cmake --preset windows-wlx-release
    cmake --build --preset windows-wlx-release

The archive is `build/windows-wlx-release/OIViewer-wlx-x64.zip`. The WLX presets select D3D11 and do
not require the Vulkan SDK. Use `windows-wlx-debug` for Debug. Upstream's standalone presets remain
available, with `OIV_BUILD_WLX=OFF`. Executable and plugin builds use separate build directories.

Extract into a dedicated directory, keeping `OIViewer.wlx64`, dependent DLLs, and `Resources` together.
Release binaries require the matching Microsoft Visual C++ runtime.

- Double Commander: add `OIViewer.wlx64` under Options > Plugins > Lister plugins.
- Total Commander: add the plugin under Configuration > Options > Plugins > Lister plugins.
  Its INI stores the nominal `OIViewer.wlx` path in `[ListerPlugins]` and marks that index with `1`
  in `[ListerPlugins64]`. The 64-bit host resolves that path to `OIViewer.wlx64`.

## Hosting behavior

The detection expression is `MULTIMEDIA & (EXT="..."|EXT="..."|...)`, using the codec extension list.
Small host buffers receive only complete rules with a closing parenthesis and terminator, or an empty result
when no rule fits. Total Commander therefore considers automatic detection in multimedia mode.

The host owns a stable child container. OIViewer owns its composite window and per-viewer renderer.
Folder navigation remains available. A host replacement changes the displayed file, while failed
replacement preserves the current image. Copy and image commands act on the viewer's current image.

Host and plugin navigation are intentionally independent. Double Commander's filename and file counter
track its own list position, which can differ from OIViewer's current image after local browsing. Host
Next/Previous continues from that host position and can reload an image already reached in OIViewer,
producing no visible image change. Use host navigation when its filename and counter must track the image.
WLX has no general filename-changed notification, and this plugin does not modify the host to add one.

Keyboard bindings take priority locally. Unbound keys are posted to the host with their original
Windows message and repeat flags. Each key keeps its recipient until release, and leaving the viewer
clears routing. A matching binding stays local even when its command fails or is disabled.
To expose a host shortcut, remove its binding from the loaded plugin's
`Resources/Configuration/KeyBindings.json` and reopen the viewer. Key bindings load at viewer creation.
Host and plugin keymaps are not synchronized automatically.

Alt+Enter toggles fullscreen. Escape restores the fullscreen viewer, or returns to the host when
docked. Fullscreen moves the existing composite into a temporary shell, retaining its canvas and image
state. Closing the host also closes an escaped viewer.

Tray, desktop file associations, window-size commands, launching another OIViewer process, and the
optional NetSettings dialog are disabled while hosted. Search and printing are not exported.

Previews decode on the CPU, apply orientation, select the first displayable image, and fit without
enlargement onto an opaque white bitmap of the requested size. The host owns the returned bitmap.
First-frame and preview flags are decoder hints and do not guarantee bounded decoder work.

Viewer operations use the host UI thread. Independent preview calls can run on worker threads with
separate loaders. Hosted viewers use native mouse events and capture without registering raw input.
The host and other plugins retain their existing process-wide mouse registration throughout viewer lifetime.
The standalone application continues to use raw mouse input.

The plugin adopts host DPI and COM state. It does not change process-wide DPI, DLL-search settings,
or the working directory. Codec dependencies load eagerly with the plugin.

## Resources and settings

Bundled fonts, cursors, commands, key bindings, and defaults are resolved relative to the viewer module.
Standalone uses `%APPDATA%/OIViewer`. Both lister hosts share `%APPDATA%/OIViewer.WLX`, with independent
`Config`, `State`, and `Cache` directories. Each profile initializes `Config/Settings.json` from its bundled
`Resources/Configuration/Settings.json` when missing, then preserves user edits. Both files use the same
schema and loading code, without inheritance, automatic copying between profiles, or synchronization.
Settings reload watches the selected profile's file. Host INI paths do not select storage.

Cache-version locks are also independent, so different standalone and WLX versions can run together.
Different WLX versions still share the WLX profile and its existing version-lock restriction. Windows
file-association checks and registration commands remain disabled in the plugin, regardless of its settings.

## Validation

    build/windows-wlx-release/bin/tests.exe "[wlx],[renderer],[oiviewer],[appcore],[storage]"
    build/windows-wlx-release/bin/LWSTests.exe
    python Tests/Scripts/check_wlx_plugin.py build/windows-wlx-release/bin/OIViewer.wlx64
    python Tests/Scripts/check_file_manager_wlx.py totalcmd C:/totalcmd/TOTALCMD64.EXE build/windows-wlx-release/bin/OIViewer.wlx64
    python Tests/Scripts/check_file_manager_wlx.py doublecmd D:/dev/doublecmd/doublecmd.exe build/windows-wlx-release/bin/OIViewer.wlx64

Enable `OIV_BUILD_TESTS=ON` and `LWS_BUILD_TESTS=ON` when configuring Release for validation.
Use `--require-cold-cache` with the native host test on a fresh WLX profile to verify first-use shader
compilation from packaged resources. This check refuses an existing generated cache and does not delete it.
The native host test covers previews, two viewers, failed replacement, resize, fullscreen, parent destruction,
raw-input ownership, keyboard routing, and DLL unload and reload. Both actual-host tests create isolated
temporary profiles and plugin copies to verify N/P forwarding without changing installed keymaps.
Total Commander navigation is tested with multiple selected images.
Both frontends link the internal `oivviewer` static library. The standalone frontend creates an LWS
context with `DispatchMode::OwnedLoop`, retaining the waitable-event dispatcher. WLX selects
`DispatchMode::ExternalLoop`, whose message-only window is serviced by the host. The dispatcher is selected
once during context initialization, with both implementations available in the same LWS build. WLX never
calls `RunMessageLoop` or `ProcessMessages`, which reject external-loop contexts.

## Presentation ownership

Each WLX entry owns an LWS child container created directly under the host HWND through `Win32::CreateChild`.
The host stays outside the LWS window family and is never wrapped or subclassed by LWS. The container's normal
resize and destruction events drive viewer layout and teardown, including closing an escaped fullscreen viewer.
Borrowed windows were not retained because their observer, read-only and detachment policy complicates every
window without solving Linux's separate GTK/Qt hosting contract. Every LWS window owns its native resource.
Each entry owns a `ViewerSession` before its attached `Viewer`. The session owns CPU image and
presentation state, animation decisions, and browsing/loading coordination. It owns watcher and residency
services and stops their producers during session teardown. The viewer owns native widgets, GPU registrations,
transient gestures, and timers.
The standalone `ViewerApplication` composes the same two components and supplies desktop services through
explicit shell callbacks. WLX does not construct tray, settings-dialog, or file-association services.

Fullscreen hosting and native focus and capture translation live in Win32 adapters. Reparenting preserves
the existing canvas and renderer. Destroying a surface detaches its completion notification and releases GPU resources,
while CPU pixels, committed selection, view choices, and pending results remain in the session. Detached
sessions retain completion batches until a presenter drains them, preserving the existing UI-thread mutation
order. Final session destruction stops producers before releasing controller references and queued results.

Linux standalone continues to use the Wayland backend. A Linux Double Commander plugin is not implemented.
Its GTK/Qt host-widget integration belongs in a separate frontend adapter. LWS's X11 backend is currently a
stub, and Wayland hosting requires toolkit-compatible surface integration rather than arbitrary foreign-window
reparenting. Session geometry uses image and viewport pixels, allowing a future terminal presenter to consume
CPU results without an LWS window or GPU renderer.
