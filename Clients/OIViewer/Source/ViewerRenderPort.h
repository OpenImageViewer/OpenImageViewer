#pragma once

#include "OIVCommands.h"
#include "ApiGlobal.h"
#include <RendererCore/Renderer.h>

#include <LWS/WindowTypes.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace OIV
{
    class IViewerRenderPort
    {
      public:

        virtual ~IViewerRenderPort() = default;

        // Bind the native canvas before configuration; SetViewportSize supplies confirmed dimensions later.
        virtual void Initialize(std::size_t canvasHandle, void* nativeDisplay = nullptr,
                                const RendererOptions& rendering = {})                           = 0;
        virtual Renderer& GetRenderer()                                                          = 0;
        virtual ResultCode Refresh()                                                             = 0;
        virtual void SetSelectionRect(const LLUtils::RectI32& rect)                              = 0;
        virtual void ClearSelectionRect()                                                        = 0;
        virtual ResultCode SetColorExposure(const OIV_CMD_ColorExposure_Request& exposure)       = 0;
        virtual ResultCode SetTexelGrid(const CmdRequestTexelGrid& grid)                         = 0;
        virtual ResultCode SetViewportSize(const LWS::PixelSize& size)                           = 0;
        virtual ResultCode RegisterCallbacks(const OIV_CMD_RegisterCallbacks_Request& callbacks) = 0;
    };

    class OivRenderGateway final : public IViewerRenderPort
    {
      public:

        ~OivRenderGateway() override
        {
            if (fInitializationAttempted)
                OIVCommands::ExecuteCommand(OIV_CMD_Destroy, &OIVCommands::NullCommand, &OIVCommands::NullCommand);
        }

        void Initialize(std::size_t canvasHandle, void* nativeDisplay = nullptr,
                        const RendererOptions& rendering = {}) override
        {
            // Whether gateway destruction must release a partial or initialized API.
            fInitializationAttempted = true;
            OIVCommands::Init(canvasHandle, nativeDisplay, rendering);
        }

        Renderer& GetRenderer() override { return *ApiGlobal::sPictureRenderer->GetRenderer(); }
        ResultCode Refresh() override { return OIVCommands::Refresh(); }

        void SetSelectionRect(const LLUtils::RectI32& rect) override { OIVCommands::SetSelectionRect(rect); }

        void ClearSelectionRect() override { OIVCommands::CancelSelectionRect(); }

        ResultCode SetColorExposure(const OIV_CMD_ColorExposure_Request& exposure) override
        {
            OIV_CMD_ColorExposure_Request request = exposure;
            return OIVCommands::ExecuteCommand(OIV_CMD_ColorExposure, &request, &OIVCommands::NullCommand);
        }

        ResultCode SetTexelGrid(const CmdRequestTexelGrid& grid) override
        {
            CmdRequestTexelGrid request = grid;
            return OIVCommands::ExecuteCommand(CE_TexelGrid, &request, &OIVCommands::NullCommand);
        }

        ResultCode SetViewportSize(const LWS::PixelSize& size) override
        {
            if (fViewportSize == size)
                return RC_Success;
            CmdSetClientSizeRequest request{static_cast<uint16_t>(size.x), static_cast<uint16_t>(size.y)};
            const ResultCode result = OIVCommands::ExecuteCommand(CMD_SetClientSize, &request,
                                                                  &OIVCommands::NullCommand);
            if (result == RC_Success)
                // Last successfully applied canvas size, used to suppress duplicate updates.
                fViewportSize = size;
            return result;
        }

        ResultCode RegisterCallbacks(const OIV_CMD_RegisterCallbacks_Request& callbacks) override
        {
            OIV_CMD_RegisterCallbacks_Request request = callbacks;
            return OIVCommands::ExecuteCommand(OIV_CMD_RegisterCallbacks, &request, &OIVCommands::NullCommand);
        }

      private:

        // Last successfully applied canvas size, used to suppress duplicate updates.
        std::optional<LWS::PixelSize> fViewportSize;
        // Whether gateway destruction must release a partial or initialized API.
        bool fInitializationAttempted{};
    };
}  // namespace OIV
