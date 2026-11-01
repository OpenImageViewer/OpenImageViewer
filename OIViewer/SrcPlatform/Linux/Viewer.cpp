#include "Viewer.h"

#include "ViewerPlatformState.h"

#include <LLUtils/Exception.h>
#include <LLUtils/PlatformUtility.h>
#include <OIVAppCore/ViewCommandPolicy.h>

#include <cstdlib>
#include <filesystem>

#ifdef LWS_HAS_WAYLAND_BACKEND
    #include <LWS/Wayland/WindowExtensions.hpp>
#endif

namespace OIV
{
    std::unique_ptr<IFileWatcher> CreateViewerFileWatcher()
    {
        return {};
    }

    void Viewer::InitializePlatformState()
    {
        fRawInputState.reset(new RawInputState());
        fPresentationReady = false;
    }

    void Viewer::RawInputStateDeleter::operator()(RawInputState* state) const noexcept
    {
        delete state;
    }

    void Viewer::InitializeRenderer(const RendererOptions& rendering)
    {
        auto& canvas  = fWindow.GetCanvasWindow();
        auto created  = CreateRenderer({reinterpret_cast<LWS::Handle>(*LWS::Wayland::GetSurface(canvas)),
                                        *LWS::Wayland::GetDisplay(canvas)},
                                       fUserDirectories.GeneratedCache(), rendering);
        fRenderer     = std::move(created.renderer);
        fRendererType = created.type;
    }

    WindowSizeDecision Viewer::GetWindowSizeDecision(const CommandManager::CommandArgs& args) const
    {
        const auto& workRect             = fCurrentMonitorProperties.workRect;
        const auto workAreaTopLeft       = workRect.GetCorner(LLUtils::TopLeft);
        const auto workAreaBottomRight   = workRect.GetCorner(LLUtils::BottomRight);
        const WindowWorkingArea workArea = {.left   = workAreaTopLeft.x,
                                            .top    = workAreaTopLeft.y,
                                            .right  = workAreaBottomRight.x,
                                            .bottom = workAreaBottomRight.y};
        const auto placement             = fWindow.GetWindow().GetPlacement();
        return ViewCommandPolicy::DecideWindowSize(args, {placement.clientSize.x, placement.clientSize.y},
                                                   placement.position.value_or(LWS::Point{}), workArea);
    }

}  // namespace OIV
