target_compile_definitions(OIViewer PRIVATE OIVIEWER_PLATFORM_LINUX)

if(NOT TARGET PkgConfig::GTK3)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
endif()
pkg_check_modules(GIOUnix REQUIRED IMPORTED_TARGET gio-unix-2.0)
target_link_libraries(OIViewer PRIVATE PkgConfig::GTK3 PkgConfig::GIOUnix)

target_sources(OIViewer PRIVATE
    ClipboardSetup.cpp
    ContextMenuBackend.cpp
    EntryPoint.cpp
    ExceptionHandler.cpp
    ExceptionHandler.h
    FileAssociations.cpp
    FileAssociations.h
    ImageControl.cpp
    LinuxKeyBindings.cpp
    LinuxKeyBindings.h
    MainWindow.cpp
    MonitorProvider.cpp
    PlatformFileDialog.cpp
    ShellCommandHandler.cpp
    ViewerApplication.cpp
    ViewerApplicationAssociations.cpp
    ViewerApplicationImage.cpp
    ViewerApplicationInput.cpp
    ViewerApplicationPlatformState.h
    ViewerApplicationUI.cpp
)
