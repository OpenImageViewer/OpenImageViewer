"""Exercise the WLX ABI in a native Windows host, using only windows created by this process.
Usage: python Tests/Scripts/check_wlx_plugin.py path/to/OIViewer.wlx64
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import os
import re
from pathlib import Path
import struct
import tempfile
import time

u = c.WinDLL("user32", use_last_error=True)
k = c.WinDLL("kernel32", use_last_error=True)
g = c.WinDLL("gdi32", use_last_error=True)
def bind(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype, fn.argtypes = restype, argtypes
    return fn

CreateWindow = bind(u, "CreateWindowExW", w.HWND, w.DWORD, w.LPCWSTR, w.LPCWSTR, w.DWORD,
                    c.c_int,c.c_int,c.c_int,c.c_int,w.HWND,w.HMENU,w.HINSTANCE,c.c_void_p)
DestroyWindow = bind(u, "DestroyWindow", w.BOOL, w.HWND)
GetParent = bind(u, "GetParent", w.HWND, w.HWND)
FindChild = bind(u, "FindWindowExW", w.HWND, w.HWND,w.HWND,w.LPCWSTR,w.LPCWSTR)
IsWindow = bind(u, "IsWindow", w.BOOL, w.HWND)
MoveWindow = bind(u, "MoveWindow", w.BOOL, w.HWND,c.c_int,c.c_int,c.c_int,c.c_int,w.BOOL)
Send = bind(u, "SendMessageW", c.c_ssize_t, w.HWND,w.UINT,w.WPARAM,w.LPARAM)
Peek = bind(u, "PeekMessageW", w.BOOL, c.POINTER(w.MSG),w.HWND,w.UINT,w.UINT,w.UINT)
Dispatch = bind(u, "DispatchMessageW", c.c_ssize_t, c.POINTER(w.MSG))
Translate = bind(u, "TranslateMessage", w.BOOL, c.POINTER(w.MSG))
Text = bind(u, "GetWindowTextW", c.c_int, w.HWND,w.LPWSTR,c.c_int)
GetRect = bind(u, "GetClientRect", w.BOOL,w.HWND,c.POINTER(w.RECT))
DeleteObject = bind(g, "DeleteObject", w.BOOL, w.HANDLE)
FreeLibrary = bind(k, "FreeLibrary", w.BOOL, w.HMODULE)
GetRegistered = None

class Device(c.Structure):
    _fields_ = [("page",w.USHORT),("usage",w.USHORT),("flags",w.DWORD),("target",w.HWND)]
GetRegistered = bind(u,"GetRegisteredRawInputDevices",w.UINT,c.POINTER(Device),c.POINTER(w.UINT),w.UINT)
RegisterRaw = bind(u,"RegisterRawInputDevices",w.BOOL,c.POINTER(Device),w.UINT,w.UINT)

def require(value, message):
    if not value:
        raise RuntimeError(f"{message} (Win32 error {c.get_last_error()})")

def pump(seconds=.15):
    deadline = time.monotonic()+seconds
    while True:
        message=w.MSG()
        while Peek(c.byref(message),None,0,0,1):
            require(message.message != 0x12,"Plugin must not quit the host")
            Translate(c.byref(message)); Dispatch(c.byref(message))
        if time.monotonic() >= deadline: return
        time.sleep(.005)

def title(hwnd):
    text=c.create_unicode_buffer(4096);Text(hwnd,text,len(text));return text.value

def mouse_target():
    count=w.UINT()
    require(GetRegistered(None,c.byref(count),c.sizeof(Device)) != 0xffffffff,"Query raw input")
    if not count.value: return None
    devices=(Device*count.value)()
    require(GetRegistered(devices,c.byref(count),c.sizeof(Device)) != 0xffffffff,"Read raw input")
    return next((d.target for d in devices if d.page==1 and d.usage==2),None)

def fullscreen(window):
    # KeyCombination reads the GUI thread's modifier state. Do not inject physical keyboard input.
    get = bind(u,"GetKeyboardState",w.BOOL,c.POINTER(c.c_ubyte))
    set_ = bind(u,"SetKeyboardState",w.BOOL,c.POINTER(c.c_ubyte))
    keys=(c.c_ubyte*256)();require(get(keys),"Read keyboard state")
    modified=(c.c_ubyte*256).from_buffer_copy(keys)
    for modifier in (0x10,0x11,0x12,0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0x5B,0x5C):
        modified[modifier]=0
    modified[0x12]=modified[0xA4]=0x80
    require(set_(modified),"Set test-thread modifiers")
    try:
        Send(window,0x104,0x0D,1|(0x1c<<16)|(1<<29))
        Send(window,0x105,0x0D,1|(0x1c<<16)|(1<<29)|(3<<30))
    finally: set_(keys)
    pump()

def mouse_capture_routing(host, window):
    set_capture=bind(u,"SetCapture",w.HWND,w.HWND)
    release_capture=bind(u,"ReleaseCapture",w.BOOL)
    dock=GetParent(window)
    position=20|(20<<16)
    Send(window,0x201,1,position)
    set_capture(host)
    Send(window,0x202,0,position)
    release_capture()
    # Native DBLCLK is still one press. The interrupted first click must not complete a viewer double-click.
    Send(window,0x203,1,position)
    Send(window,0x202,0,position)
    pump(.6)
    require(GetParent(window)==dock,"Stolen capture cancels pending double-click history")
    Send(window,0x201,1,position)
    Send(window,0x202,0,position)
    Send(window,0x203,1,position)
    Send(window,0x202,0,position)
    pump()
    require(GetParent(window)!=dock,"Normal capture release preserves double-click fullscreen")
    Send(window,0x100,0x1B,1)
    Send(window,0x101,0x1B,1|(3<<30))
    pump()
    require(GetParent(window)==dock,"Escape restores after mouse fullscreen")

def keyboard_routing(host, window):
    # Record messages at the host without changing any installed keymap or host profile.
    comctl=c.WinDLL("comctl32")
    callback=c.WINFUNCTYPE(c.c_ssize_t,w.HWND,w.UINT,w.WPARAM,w.LPARAM,c.c_size_t,c.c_size_t)
    default=bind(comctl,"DefSubclassProc",c.c_ssize_t,w.HWND,w.UINT,w.WPARAM,w.LPARAM)
    install=bind(comctl,"SetWindowSubclass",w.BOOL,w.HWND,callback,c.c_size_t,c.c_size_t)
    remove=bind(comctl,"RemoveWindowSubclass",w.BOOL,w.HWND,callback,c.c_size_t)
    received=[]
    @callback
    def record(hwnd,message,key,data,id,reference):
        if message in (0x100,0x101,0x104,0x105):
            received.append((message,key,data & 0xffffffff))
            return 0
        return default(hwnd,message,key,data)
    require(install(host,record,1,0),"Observe host keyboard messages")
    try:
        # F24 is unbound in the bundled keymap. Preserve system-key identity and all flag bits.
        events=[(0x104,0x87,1|(0x76<<16)|(1<<29)),
                (0x104,0x87,2|(0x76<<16)|(1<<29)|(1<<30)),
                (0x105,0x87,1|(0x76<<16)|(1<<29)|(3<<30))]
        for message,key,data in events: Send(window,message,key,data)
        require(not received,"Host forwarding must be asynchronous")
        pump()
        require(received==events,"Unbound gesture reaches host exactly once with native flags")
        received.clear()
        # Alt+Enter is local. Its release must not leak to the host.
        fullscreen(window)
        require(not received,"Bound fullscreen gesture stays local")
        for message,key,data in events: Send(window,message,key,data)
        pump()
        require(received==events,"Fullscreen forwarding still reaches the original host")
        received.clear()
        fullscreen(window)
        received.clear()
        # A local key must not switch to the host when modifiers change during a repeat.
        Send(window,0x100,ord("N"),1)
        get=bind(u,"GetKeyboardState",w.BOOL,c.POINTER(c.c_ubyte))
        set_=bind(u,"SetKeyboardState",w.BOOL,c.POINTER(c.c_ubyte))
        keys=(c.c_ubyte*256)()
        require(get(keys),"Read test-thread modifiers")
        modified=(c.c_ubyte*256).from_buffer_copy(keys)
        modified[0x10]=modified[0xA0]=0x80
        require(set_(modified),"Change repeat modifiers")
        try:
            Send(window,0x100,ord("N"),1|(1<<30))
            Send(window,0x101,ord("N"),1|(3<<30))
        finally:
            set_(keys)
        pump()
        require(not received,"Local gesture stays local across modifier changes")
        Send(window,0x100,0x87,1)
        pump()
        require(len(received)==1,"Unbound key down reaches host")
        received.clear()
        Send(window,0x8,host,0)
        Send(window,0x101,0x87,1|(3<<30))
        pump()
        require(not received,"External focus loss clears key routing")
    finally:
        remove(host,record,1)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plugin",type=Path)
    parser.add_argument("--require-cold-cache", action="store_true",
                        help="Require a fresh WLX generated cache without deleting existing user data")
    args=parser.parse_args()
    plugin=args.plugin.resolve(strict=True)
    if args.require_cold_cache:
        appdata=c.create_unicode_buffer(32768)
        folder=bind(c.WinDLL("shell32"),"SHGetFolderPathW",c.c_long,
                    w.HWND,c.c_int,w.HANDLE,w.DWORD,w.LPWSTR)
        require(folder(None,26,None,0,appdata)==0,"Resolve roaming AppData")
        cache=Path(appdata.value)/"OIViewer.WLX"/"Cache"/"Generated"
        # A warm shader cache can hide resource lookups against the host executable's directory.
        # Check before loading any viewer and leave existing caches untouched.
        require(not cache.exists(),"Cold-cache verification requires a fresh WLX generated cache")
    cwd=Path.cwd()
    with tempfile.TemporaryDirectory(prefix="oiv-wlx-") as temp:
        temp=Path(temp)
        first=temp/"first-\u05d0.bmp"
        second=temp/"second.bmp"
        # Tiny bottom-up BMPs give different current-file identities without external test dependencies.
        def bmp(color):
            return b"BM"+struct.pack("<IHHI",70,0,0,54)+struct.pack("<IiiHHIIiiII",40,2,2,1,24,0,16,0,0,0,0)+bytes(color)*2+b"\0\0"+bytes(color)*2+b"\0\0"
        first.write_bytes(bmp((0,0,255)));second.write_bytes(bmp((255,0,0)))
        icons=temp/"pages.ico"
        def icon(color):
            return struct.pack("<IiiHHIIiiII",40,2,4,1,32,0,24,0,0,0,0)+bytes(color)*4+bytes(8)
        red,blue=icon((0,0,255,255)),icon((255,0,0,255))
        icons.write_bytes(struct.pack("<HHH",0,1,2)+
            struct.pack("<BBBBHHII",2,2,0,0,1,32,len(red),38)+
            struct.pack("<BBBBHHII",2,2,0,0,1,32,len(blue),38+len(red))+red+blue)
        for iteration in range(3):
            dll=c.WinDLL(str(plugin),winmode=0x100|0x1000)
            load=bind(dll,"ListLoadW",w.HWND,w.HWND,w.LPWSTR,c.c_int)
            next_=bind(dll,"ListLoadNextW",c.c_int,w.HWND,w.HWND,w.LPWSTR,c.c_int)
            load_ansi=bind(dll,"ListLoad",w.HWND,w.HWND,c.c_char_p,c.c_int)
            next_ansi=bind(dll,"ListLoadNext",c.c_int,w.HWND,w.HWND,c.c_char_p,c.c_int)
            preview_ansi=bind(dll,"ListGetPreviewBitmap",w.HBITMAP,c.c_char_p,c.c_int,c.c_int,c.c_void_p,c.c_int)
            close=bind(dll,"ListCloseWindow",None,w.HWND)
            command=bind(dll,"ListSendCommand",c.c_int,w.HWND,c.c_int,c.c_int)
            preview=bind(dll,"ListGetPreviewBitmapW",w.HBITMAP,w.LPWSTR,c.c_int,c.c_int,c.c_void_p,c.c_int)
            detect=bind(dll,"ListGetDetectString",None,c.c_void_p,c.c_int)
            for omitted in ("ListPrint","ListPrintW","ListSearchText","ListSearchTextW","ListSearchDialog"):
                require(not hasattr(dll,omitted),f"{omitted} must not advertise unsupported functionality")
            buffer=c.create_string_buffer(8192);detect(buffer,len(buffer))
            require(b'EXT="BMP"' in buffer.value,"Detect string advertises BMP")
            rule=rb'MULTIMEDIA & \(EXT="[A-Z0-9]+"(?:\|EXT="[A-Z0-9]+")*\)'
            require(re.fullmatch(rule,buffer.value),"Multimedia detection has a complete extension group")
            detect(None,0)
            for capacity in (-1,0,1,2,14,21,22,23,24,32,64,128,1024,len(buffer.value),len(buffer.value)+1):
                size=max(capacity,0)
                guarded=c.create_string_buffer(b"\x7f"*(size+8),size+8)
                detect(guarded,capacity)
                require(guarded.raw[size:]==b"\x7f"*8,"Detect respects the buffer capacity")
                if capacity>0:
                    contents=guarded.raw[:size]
                    require(b"\0" in contents,"Detect result is null-terminated")
                    value=contents.split(b"\0",1)[0]
                    require(not value or re.fullmatch(rule,value),"Small detection buffers contain complete rules")
            bitmap=preview(str(first),64,48,None,0)
            require(bitmap,"Preview before viewer initialization");require(DeleteObject(bitmap),"Host owns preview")
            require(not preview(str(temp/"missing.png"),64,48,None,0),"Invalid preview")
            bitmap=preview_ansi(str(second).encode("mbcs"),32,32,None,0)
            require(bitmap,"ANSI preview");require(DeleteObject(bitmap),"ANSI bitmap ownership")
            require(not load(None,str(first),2),"Reject null parent")
            host=CreateWindow(0,"STATIC","OIV WLX integration test",0x10CF0000,80,80,800,600,None,None,None,None)
            require(host,"Create host")
            a=b=None
            original_mouse=Device(1,2,0x100,host)
            require(RegisterRaw(c.byref(original_mouse),1,c.sizeof(Device)),"Register host mouse before loading WLX")
            try:
                a=load(host,str(first),2|64);require(a,"Load first viewer")
                require(mouse_target()==host,"Loading WLX preserves the existing host mouse registration")
                composite=FindChild(a,None,"LWS_WINDOW_CLASS",None);require(composite,"Composite child")
                canvas=FindChild(composite,None,"LWS_WINDOW_CLASS",None);require(canvas,"Canvas child")
                require(GetParent(a)==host,"Stable host container")
                pump()
                require(first.stem in title(composite),"First image displayed")
                keyboard_routing(host,composite)
                mouse_capture_routing(host,composite)
                b=load_ansi(host,str(second).encode("mbcs"),2|64);require(b,"Load independent second viewer")
                other=FindChild(b,None,"LWS_WINDOW_CLASS",None)
                pump()
                require(second.stem in title(other),"Second image displayed")
                require(first.stem in title(composite),"First viewer state preserved")
                require(next_(host,b,str(icons),2)==0,"Load a multi-image file")
                pump()
                first_child=FindChild(other,None,"LWS_WINDOW_CLASS",None)
                require(FindChild(other,first_child,"LWS_WINDOW_CLASS",None),"Sidebar shares the composite parent")
                require(mouse_target()==host,"Viewers leave host raw input registration intact")
                require(next_(host,a,str(temp/"missing.png"),2)==1,"Failed replacement reports failure")
                require(first.stem in title(composite),"Failed replacement retains image")
                require(next_ansi(host,a,str(second).encode("mbcs"),2)==0,"ANSI replacement")
                pump()
                require(second.stem in title(composite),"Replacement displayed")
                require(command(a,3,0)==0,"Select all")
                require(command(a,4,50)==0,"Scroll percent")
                require(command(a,999,0)==1,"Unknown command fails")
                MoveWindow(a,0,0,430,310,True);pump()
                rect=w.RECT();GetRect(composite,c.byref(rect))
                require((rect.right,rect.bottom)==(430,310),"Host resizing reaches composite")
                fullscreen(composite)
                require(GetParent(a)==host,"Container remains docked during fullscreen")
                require(GetParent(composite)!=a,"Composite escaped to fullscreen shell")
                set_focus=bind(u,"SetFocus",w.HWND,w.HWND)
                get_focus=bind(u,"GetFocus",w.HWND)
                set_focus(GetParent(composite))
                require(get_focus()==composite,"Fullscreen activation focuses the composite")
                require(IsWindow(canvas),"Canvas handle survives fullscreen")
                require(next_(host,a,str(first),2)==0,"Replace file while fullscreen")
                Send(composite,0x100,0x1B,1);pump()
                require(GetParent(composite)==a,"Escape restores composite")
                require(IsWindow(canvas),"Canvas handle survives restore")
                # Another component may replace our process registration. Closing OIV must preserve it.
                replacement=Device(1,2,0x100,host)
                require(RegisterRaw(c.byref(replacement),1,c.sizeof(Device)),"Replace raw-input registration")
                close(b);b=None;pump()
                require(IsWindow(a),"Closing second viewer preserves first")
                fullscreen(composite)
                close(a);a=None;pump()
                require(not IsWindow(composite),"Closing escaped viewer destroys composite")
                require(mouse_target()==host,"Cleanup preserves another component's registration")
                remove=Device(1,2,1,None);RegisterRaw(c.byref(remove),1,c.sizeof(Device))
                a=load(host,str(first),2);require(a,"Reopen after last close");pump()
                reopened=FindChild(a,None,"LWS_WINDOW_CLASS",None)
                fullscreen(reopened)
                Send(reopened,0x100,0x20,1|(0x39<<16))
                Send(reopened,0x101,0x20,1|(0x39<<16)|(3<<30))
                time.sleep(2.1)
                stale_tick=w.MSG()
                require(Peek(c.byref(stale_tick),reopened,0x113,0x113,1),"Retrieve a pending slideshow timer")
                require(stale_tick.lParam==0,"Hosted timer contains no DLL callback address")
                DestroyWindow(host);host=None;pump()
                require(not IsWindow(reopened),"Parent destruction also closes an escaped composite")
                close(a);a=None;pump()
                require(mouse_target() is None,"WLX does not register a raw mouse receiver")
                require(Path.cwd()==cwd,"Plugin preserves working directory")
                print(f"PASS cycle {iteration+1}: previews, two viewers, replacement, resize, fullscreen, teardown",flush=True)
            finally:
                if b: close(b)
                if a: close(a)
                remove=Device(1,2,1,None);RegisterRaw(c.byref(remove),1,c.sizeof(Device))
                if host: DestroyWindow(host)
                pump()
            require(FreeLibrary(dll._handle),"Unload plugin")
            Dispatch(c.byref(stale_tick))
            print(f"PASS cycle {iteration+1}: DLL unload",flush=True)
    return 0

if __name__=="__main__":
    raise SystemExit(main())
