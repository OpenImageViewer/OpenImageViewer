target_compile_definitions(OIViewer PRIVATE OIVIEWER_PLATFORM_WIN32)

target_sources(OIViewer PRIVATE
    ClipboardSetup.cpp
    ContextMenuBackend.cpp
    CopyDataProtocol.h
    EntryPoint.cpp
    ExceptionHandler.cpp
    ExceptionHandler.h
    FileWatcherWin32.h
    Helpers/PhotoshopFinder.h
    Helpers/ShellCommandHandler.cpp
    ImageControl.cpp
    MainWindow.cpp
    MonitorProvider.cpp
    PlatformFileDialog.cpp
    OIViewerManifest.xml
    Resource.h
    ViewerApplication.cpp
    ViewerApplicationImage.cpp
    ViewerApplicationInput.cpp
    ViewerApplicationPlatformState.h
    ViewerApplicationUI.cpp
)
