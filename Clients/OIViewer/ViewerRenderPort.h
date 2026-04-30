#pragma once

#include "OIVCommands.h"

#include <Windows.h>

#include <cstdint>

namespace OIV
{
    class IViewerRenderPort
    {
      public:
        virtual ~IViewerRenderPort() = default;

        virtual void Initialize(HANDLE canvasHandle) = 0;
        virtual ResultCode Refresh() = 0;
        virtual void SetSelectionRect(const LLUtils::RectI32& rect) = 0;
        virtual void ClearSelectionRect() = 0;
        virtual ResultCode SetColorExposure(const OIV_CMD_ColorExposure_Request& exposure) = 0;
        virtual ResultCode SetTexelGrid(const CmdRequestTexelGrid& grid) = 0;
        virtual ResultCode SetClientSize(uint16_t width, uint16_t height) = 0;
        virtual ResultCode RegisterCallbacks(const OIV_CMD_RegisterCallbacks_Request& callbacks) = 0;
    };

    class OivRenderGateway final : public IViewerRenderPort
    {
      public:
        void Initialize(HANDLE canvasHandle) override
        {
            OIVCommands::Init(canvasHandle);
        }

        ResultCode Refresh() override
        {
            return OIVCommands::Refresh();
        }

        void SetSelectionRect(const LLUtils::RectI32& rect) override
        {
            OIVCommands::SetSelectionRect(rect);
        }

        void ClearSelectionRect() override
        {
            OIVCommands::CancelSelectionRect();
        }

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

        ResultCode SetClientSize(uint16_t width, uint16_t height) override
        {
            CmdSetClientSizeRequest request{width, height};
            return OIVCommands::ExecuteCommand(CMD_SetClientSize, &request, &OIVCommands::NullCommand);
        }

        ResultCode RegisterCallbacks(const OIV_CMD_RegisterCallbacks_Request& callbacks) override
        {
            OIV_CMD_RegisterCallbacks_Request request = callbacks;
            return OIVCommands::ExecuteCommand(OIV_CMD_RegisterCallbacks, &request, &OIVCommands::NullCommand);
        }
    };
}  // namespace OIV
