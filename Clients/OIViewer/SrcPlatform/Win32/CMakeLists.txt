target_compile_definitions(${TargetName} PUBLIC OIVIEWER_PLATFORM_WIN32)

target_sources(${TargetName} PRIVATE
    FileAssociations.cpp
    FileAssociationDefaults.cpp
    ClipboardSetup.cpp
    ContextMenuBackend.cpp
    CopyDataProtocol.h
    ExceptionHandler.cpp
    ExceptionHandler.h
    FileWatcherWin32.h
    Helpers/PhotoshopFinder.h
    Helpers/ShellCommandHandler.cpp
    HostedInputAdapter.cpp
    HostedWindowController.cpp
    ImageControl.cpp
    MainWindow.cpp
    MonitorProvider.cpp
    OIViewerManifest.xml
    Resource.h
    Viewer.cpp
    ViewerImage.cpp
    ViewerInput.cpp
    ViewerPlatformState.h
)

if(OIV_BUILD_WLX)
    target_sources(${FrontendTarget} PRIVATE WlxApi.h WlxPlugin.cpp WlxPreview.cpp WlxPreview.h)
else()
    target_sources(${FrontendTarget} PRIVATE EntryPoint.cpp)
endif()
