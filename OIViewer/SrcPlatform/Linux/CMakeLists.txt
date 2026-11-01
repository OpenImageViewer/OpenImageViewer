target_compile_definitions(${TargetName} PUBLIC OIVIEWER_PLATFORM_LINUX)

# GTK remains necessary for image clipboard transfer and existing confirmation/error dialogs.
# File selection is provided by LWS through the desktop portal instead of GTK.
if(NOT TARGET PkgConfig::GTK3)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
endif()
pkg_check_modules(GIOUnix REQUIRED IMPORTED_TARGET gio-unix-2.0)
target_link_libraries(${TargetName} PUBLIC PkgConfig::GTK3 PkgConfig::GIOUnix)

target_sources(${TargetName} PRIVATE
    ClipboardSetup.cpp
    ContextMenuBackend.cpp
    ExceptionHandler.cpp
    ExceptionHandler.h
    FileAssociations.cpp
    FileAssociations.h
    ImageControl.cpp
    LinuxKeyBindings.cpp
    LinuxKeyBindings.h
    MainWindow.cpp
    MonitorProvider.cpp
    ShellCommandHandler.cpp
    Viewer.cpp
    ViewerImage.cpp
    ViewerInput.cpp
    ViewerPlatformState.h
)

target_sources(${FrontendTarget} PRIVATE EntryPoint.cpp)
