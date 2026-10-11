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
#include <RendererCore/IRenderBackend.h>

namespace OIV
{
    // OpenGL graphics backend borrowing prepared common records only during rendering.
    class OIVGLRenderer : public IRenderBackend
    {
      public:

        OIVGLRenderer() = default;
        ~OIVGLRenderer() override;

        int Init(const OIV_RendererInitializationParams& initParams) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        int Render(const RenderFrame& frame) override;
        int SetFilterLevel(OIV_Filter_type filterType) override;
        int SetSelectionRect(VisualSelectionRect selectionRect) override;
        int SetExposure(const OIV_CMD_ColorExposure_Request& exposure) override;
        int SetBackgroundColor(int index, LLUtils::Color backgroundColor) override;
        void ReleaseImageResources(uint64_t id) noexcept override;

        Acceleration GetAcceleration() const override { return fAcceleration; }
        const char* GetBackendName() const override { return "GL"; }
        const char* GetGPUName() const override;
        const char* GetAPIVersion() const override;
        const char* GetDriverVersion() const override { return ""; }

      private:

        // Selected adapter acceleration classification.
        Acceleration fAcceleration = Acceleration::Unknown;

        // GPU cache keyed by renderer-local ID, with no borrowed record pointers.
        struct ImageEntry
        {
            // Last content revision successfully uploaded by this backend.
            uint64_t uploadedRevision = 0;
            // Owned lazy GPU texture, absent until a visible nonempty record uploads.
            std::unique_ptr<GLTexture> texture;
            // Successfully uploaded texture width in texels.
            uint32_t width{};
            // Successfully uploaded texture height in rows.
            uint32_t height{};
        };

        void DrawImage(ImageEntry& entry, const RenderImageRecord& record);
        void DrawSelectionRect() const;
        void PrepareResources();
        void RenderImages(const RenderFrame& frame, OIV_Image_Render_mode renderMode);
        void UpdateGpuParams(const ImageEntry& entry, const RenderImageRecord& record) const;
        void UpdateViewportSize(int width, int height);

        // Owned graphics context, released before the native canvas is destroyed.
        GLContext fContext;
        // Owned main-image and overlay shader program.
        GLGpuProgramUniquePtr fProgram;
        // Owned selection shader program.
        GLGpuProgramUniquePtr fSelectionProgram;
        // Owned fullscreen quad geometry.
        std::unique_ptr<Quad> fQuad;
        // Renderer-local GPU cache with successful upload revisions and retirement ownership.
        std::map<uint64_t, ImageEntry> fImageEntries;
        // Owned OpenGL vertex-array name.
        GLuint fVertexArray{};
        // Client viewport dimensions in pixels.
        std::array<float, 2> fViewportSize{};
        static constexpr std::array<float, 4> DefaultCanvasColor{45.0F / 255.0F, 45.0F / 255.0F, 48.0F / 255.0F, 1.0F};
        // Existing RGBA background-pattern colors.
        std::array<std::array<float, 4>, 2> fBackgroundColors{{{0.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 0.16F, 1.0F}}};
        std::array<std::array<float, 4>, 2> fTransparencyColors{
            {{0.75F, 0.75F, 0.75F, 1.0F}, {1.0F, 1.0F, 1.0F, 1.0F}}};
        // Current client selection rectangle, with the existing absent sentinel.
        VisualSelectionRect fSelectionRect{{-1, -1}, {-1, -1}};
        // Global fallback sampling policy for the existing per-image filter behavior.
        OIV_Filter_type fFilterType{FT_Linear};
        // Existing shader exposure multiplier.
        float fExposure{1.0F};
        // Existing shader color offset.
        float fOffset{};
        // Existing shader gamma value.
        float fGamma{1.0F};
        // Existing shader saturation multiplier.
        float fSaturation{1.0F};
        // Whether the existing texel-grid visualization is enabled.
        bool fShowGrid{};
        // Cached UTF-8 selected GPU name.
        mutable std::string fGPUName;
        // Cached UTF-8 graphics API version.
        mutable std::string fAPIVersion;
    };
}  // namespace OIV
