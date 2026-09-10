#pragma once

#include <GL/glew.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>

#include "GLContext.h"
#include "GLRenderer/GLGpuProgram.h"
#include "GLRenderer/GLTexture.h"
#include "GLRenderer/Quad.h"
#include <Interfaces/IRenderer.h>

namespace OIV
{
    class OIVGLRenderer : public IRenderer
    {
      public:

        OIVGLRenderer() = default;
        ~OIVGLRenderer() override;

        int Init(const OIV_RendererInitializationParams& initParams) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        int Redraw() override;
        int SetFilterLevel(OIV_Filter_type filterType) override;
        int SetSelectionRect(VisualSelectionRect selectionRect) override;
        int SetExposure(const OIV_CMD_ColorExposure_Request& exposure) override;
        int SetBackgroundColor(int index, LLUtils::Color backgroundColor) override;
        int AddRenderable(IRenderable* renderable) override;
        int RemoveRenderable(IRenderable* renderable) override;

        Acceleration GetAcceleration() const override { return fAcceleration; }
        const char* GetBackendName() const override { return "GL"; }
        const char* GetGPUName() const override;
        const char* GetAPIVersion() const override;
        const char* GetDriverVersion() const override { return ""; }

      private:

        Acceleration fAcceleration = Acceleration::Unknown;

        struct ImageEntry
        {
            IRenderable* renderable{};
            std::unique_ptr<GLTexture> texture;
            uint32_t width{};
            uint32_t height{};
        };

        void DrawImage(ImageEntry& entry);
        void DrawSelectionRect() const;
        void PrepareResources();
        void RenderImages(OIV_Image_Render_mode renderMode);
        void UpdateGpuParams(const ImageEntry& entry) const;
        void UpdateViewportSize(int width, int height);

        GLContext fContext;
        GLGpuProgramUniquePtr fProgram;
        GLGpuProgramUniquePtr fSelectionProgram;
        std::unique_ptr<Quad> fQuad;
        std::map<uint32_t, ImageEntry> fImageEntries;
        GLuint fVertexArray{};
        std::array<float, 2> fViewportSize{};
        static constexpr std::array<float, 4> DefaultCanvasColor{45.0F / 255.0F, 45.0F / 255.0F, 48.0F / 255.0F, 1.0F};
        std::array<std::array<float, 4>, 2> fBackgroundColors{{{0.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 0.16F, 1.0F}}};
        std::array<std::array<float, 4>, 2> fTransparencyColors{
            {{0.75F, 0.75F, 0.75F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}}};
        VisualSelectionRect fSelectionRect{{-1, -1}, {-1, -1}};
        OIV_Filter_type fFilterType{FT_Linear};
        float fExposure{1.0F};
        float fOffset{};
        float fGamma{1.0F};
        float fSaturation{1.0F};
        bool fShowGrid{};
        mutable std::string fGPUName;
        mutable std::string fAPIVersion;
    };
}  // namespace OIV
