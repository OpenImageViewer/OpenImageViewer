#pragma once

#include "OIVCommands.h"

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
        virtual void ResumePresentation()                                                        = 0;
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

        enum class PresentationState
        {
            Ready,
            Deferred,
        };

        explicit OivRenderGateway(PresentationState state = PresentationState::Ready)
            : fPresentationReady(state == PresentationState::Ready)
        {
        }

        ~OivRenderGateway() override
        {
            if (fInitializationAttempted)
                OIVCommands::ExecuteCommand(OIV_CMD_Destroy, &OIVCommands::NullCommand, &OIVCommands::NullCommand);
        }

        void Initialize(std::size_t canvasHandle, void* nativeDisplay = nullptr,
                        const RendererOptions& rendering = {}) override
        {
            fInitializationAttempted = true;
            OIVCommands::Init(canvasHandle, nativeDisplay, rendering);
        }

        ResultCode Refresh() override
        {
            if (!fPresentationReady)
            {
                fRefreshPending = true;
                return RC_Success;
            }
            return OIVCommands::Refresh();
        }

        void ResumePresentation() override
        {
            fPresentationReady = true;
            if (fRefreshPending)
            {
                fRefreshPending = false;
                OIVCommands::Refresh();
            }
        }

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
                fViewportSize = size;
            return result;
        }

        ResultCode RegisterCallbacks(const OIV_CMD_RegisterCallbacks_Request& callbacks) override
        {
            OIV_CMD_RegisterCallbacks_Request request = callbacks;
            return OIVCommands::ExecuteCommand(OIV_CMD_RegisterCallbacks, &request, &OIVCommands::NullCommand);
        }

      private:

        bool fPresentationReady;
        bool fRefreshPending{};
        std::optional<LWS::PixelSize> fViewportSize;
        bool fInitializationAttempted{};
    };
}  // namespace OIV
