#include "OIVGLRenderer.h"
#include "GLAcceleration.h"

#include <ImageUtil/ImageUtil.h>

#include <stdexcept>

namespace OIV
{
    namespace
    {
        constexpr char VertexShader[] = R"glsl(
#version 130
in vec2 position;
out vec2 coords;

void main()
{
    coords = position * 0.5 + 0.5;
    coords.y = 1.0 - coords.y;
    gl_Position = vec4(position, 0.0, 1.0);
}
)glsl";

        constexpr char SelectionFragmentShader[] = R"glsl(
#version 130
in vec2 coords;
out vec4 outColor;

uniform vec2 viewportSize;
uniform vec4 selectionRect;

void main()
{
    vec2 p0 = min(selectionRect.xy, selectionRect.zw);
    vec2 p1 = max(selectionRect.xy, selectionRect.zw);
    vec2 pixel = coords * viewportSize;
    if (pixel.x < p0.x || pixel.x > p1.x || pixel.y < p0.y || pixel.y > p1.y)
    {
        outColor = vec4(1.0, 1.0, 1.0, 0.8);
        return;
    }

    float horizontalDistance = min(pixel.y - p0.y, p1.y - pixel.y);
    float verticalDistance = min(pixel.x - p0.x, p1.x - pixel.x);
    float borderDistance = min(horizontalDistance, verticalDistance);
    if (borderDistance > 1.0)
    {
        outColor = vec4(0.0);
        return;
    }

    bool horizontalEdge = horizontalDistance <= verticalDistance;
    float edgeLength = horizontalEdge ? p1.x - p0.x : p1.y - p0.y;
    float segmentLength = clamp(edgeLength * 0.1, 15.0, 100.0);
    float edgePosition = horizontalEdge ? pixel.x - p0.x : pixel.y - p0.y;
    bool firstColor = mod(edgePosition, segmentLength) < segmentLength * 0.5;
    outColor = firstColor ? vec4(0.0, 120.0 / 255.0, 215.0 / 255.0, 1.0)
                          : vec4(1.0, 1.0, 0.0, 1.0);
}
)glsl";

        constexpr char FragmentShader[] = R"glsl(
#version 130
in vec2 coords;
out vec4 outColor;

uniform sampler2D imageTexture;
uniform vec2 viewportSize;
uniform vec2 imageSize;
uniform vec2 imageScale;
uniform vec2 imageOffset;
uniform vec4 backgroundColor1;
uniform vec4 backgroundColor2;
uniform vec4 transparencyColor1;
uniform vec4 transparencyColor2;
uniform float opacity;
uniform float exposure;
uniform float colorOffset;
uniform float gamma;
uniform float saturation;
uniform int imageRenderMode;
uniform int showGrid;

void main()
{
    vec2 uvScale = viewportSize / (imageSize * imageScale);
    vec2 uv = coords * uvScale - imageOffset / viewportSize * uvScale;
    vec2 checker = mod(floor(gl_FragCoord.xy / 16.0), 2.0);
    bool firstCheckerColor = checker.x == checker.y;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
    {
        if (imageRenderMode == 1)
            outColor = firstCheckerColor ? backgroundColor1 : backgroundColor2;
        else
            discard;
        return;
    }

    vec4 sampled = texture(imageTexture, uv);
    vec3 corrected = pow(max(sampled.rgb * exposure + colorOffset, 0.0), vec3(1.0 / max(gamma, 0.0001)));
    float luminance = dot(corrected, vec3(0.299, 0.587, 0.114));
    corrected = mix(vec3(luminance), corrected, saturation);

    if (imageRenderMode == 1)
    {
        vec4 checkerColor = firstCheckerColor ? transparencyColor1 : transparencyColor2;
        outColor = vec4(mix(checkerColor.rgb, corrected, sampled.a), opacity);
    }
    else
    {
        outColor = vec4(corrected, sampled.a * opacity);
    }

    if (showGrid == 1)
    {
        vec2 pixel = abs(fract(uv * imageSize) - 0.5);
        if (max(imageScale.x, imageScale.y) >= 8.0 && (pixel.x > 0.47 || pixel.y > 0.47))
            outColor.rgb = mix(outColor.rgb, vec3(1.0, 0.25, 0.25), 0.65);
    }
}
)glsl";

        std::array<float, 4> ToFloatColor(LLUtils::Color color)
        {
            const LLUtils::ColorF32 floatColor = static_cast<LLUtils::ColorF32>(color);
            return floatColor.channels;
        }
    }  // namespace

    OIVGLRenderer::~OIVGLRenderer()
    {
        if (fVertexArray != 0)
            glDeleteVertexArrays(1, &fVertexArray);
    }

    void OIVGLRenderer::PrepareResources()
    {
        fContext.SetSwapInterval(0);

        glGenVertexArrays(1, &fVertexArray);
        glBindVertexArray(fVertexArray);

        fProgram          = std::make_unique<GLGpuProgram>(VertexShader, FragmentShader);
        fSelectionProgram = std::make_unique<GLGpuProgram>(VertexShader, SelectionFragmentShader);
        fQuad             = std::make_unique<Quad>();

        fQuad->Bind();
        for (const GLGpuProgram* program : {fProgram.get(), fSelectionProgram.get()})
        {
            const GLint positionAttribute = glGetAttribLocation(program->GetProgram(), "position");
            glVertexAttribPointer(positionAttribute, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            glEnableVertexAttribArray(positionAttribute);
        }
        fProgram->Bind();
        fProgram->SetUniform1I("imageTexture", 0);

        glEnable(GL_BLEND);
        // The canvas is cleared opaque. Blend translucent images into its colors while
        // preserving that alpha, so Wayland never composites the desktop through an overlay.
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    }

    int OIVGLRenderer::Init(const OIV_RendererInitializationParams& initParams)
    {
        if (initParams.gpuIndex >= 0 || initParams.adapterName != nullptr)
            throw std::invalid_argument(
                "GL uses the platform-selected adapter; explicit adapter selection is unsupported");
        fContext.Init(initParams.container, initParams.nativeDisplay);
        glewExperimental      = GL_TRUE;
        const GLenum glewCode = glewInit();
        if (glewCode != GLEW_OK)
            throw std::runtime_error("Could not initialize GLEW: " +
                                     std::string(reinterpret_cast<const char*>(glewGetErrorString(glewCode))));

        // GLEW can leave GL_INVALID_ENUM behind while probing a compatibility context.
        glGetError();
        const auto* vendor   = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        fAcceleration        = ClassifyGLAcceleration(vendor ? vendor : "", renderer ? renderer : "");
        PrepareResources();
        return 0;
    }

    int OIVGLRenderer::SetViewParams(const ViewParameters& viewParams)
    {
        UpdateViewportSize(static_cast<int>(viewParams.uViewportSize.x), static_cast<int>(viewParams.uViewportSize.y));
        fTransparencyColors[0] = ToFloatColor(viewParams.uTransparencyColor1);
        fTransparencyColors[1] = ToFloatColor(viewParams.uTransparencyColor2);
        fShowGrid              = viewParams.showGrid;
        return 0;
    }

    int OIVGLRenderer::Redraw()
    {
        glClearColor(DefaultCanvasColor[0], DefaultCanvasColor[1], DefaultCanvasColor[2], DefaultCanvasColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        RenderImages(IRM_MainImage);
        DrawSelectionRect();
        RenderImages(IRM_Overlay);
        fContext.SwapBuffers();
        return 0;
    }

    int OIVGLRenderer::SetFilterLevel(OIV_Filter_type filterLevel)
    {
        fFilterType = filterLevel;
        return 0;
    }

    int OIVGLRenderer::SetSelectionRect(VisualSelectionRect selectionRect)
    {
        fSelectionRect = selectionRect;
        return 0;
    }

    int OIVGLRenderer::SetExposure(const OIV_CMD_ColorExposure_Request& exposure)
    {
        fExposure   = static_cast<float>(exposure.exposure);
        fOffset     = static_cast<float>(exposure.offset);
        fGamma      = static_cast<float>(exposure.gamma);
        fSaturation = static_cast<float>(exposure.saturation);
        return 0;
    }

    int OIVGLRenderer::SetBackgroundColor(int index, LLUtils::Color backgroundColor)
    {
        fBackgroundColors.at(static_cast<size_t>(index)) = ToFloatColor(backgroundColor);
        return 0;
    }

    int OIVGLRenderer::AddRenderable(IRenderable* renderable)
    {
        if (renderable == nullptr)
            throw std::invalid_argument("Cannot add a null OpenGL renderable");

        const bool inserted = fImageEntries.emplace(renderable->GetID(), ImageEntry{.renderable = renderable}).second;
        if (!inserted)
            throw std::runtime_error("Cannot add the same OpenGL renderable twice");

        return 0;
    }

    int OIVGLRenderer::RemoveRenderable(IRenderable* renderable)
    {
        if (renderable == nullptr || fImageEntries.erase(renderable->GetID()) == 0)
            throw std::runtime_error("Cannot remove an unknown OpenGL renderable");

        return 0;
    }

    const char* OIVGLRenderer::GetGPUName() const
    {
        if (fGPUName.empty())
        {
            const GLubyte* name = glGetString(GL_RENDERER);
            if (name != nullptr)
                fGPUName = reinterpret_cast<const char*>(name);
            else
                fGPUName = "Unknown";
        }
        return fGPUName.c_str();
    }

    const char* OIVGLRenderer::GetAPIVersion() const
    {
        if (fAPIVersion.empty())
        {
            const GLubyte* version = glGetString(GL_VERSION);
            if (version != nullptr)
                fAPIVersion = reinterpret_cast<const char*>(version);
            else
                fAPIVersion = "Unknown";
        }
        return fAPIVersion.c_str();
    }

    void OIVGLRenderer::RenderImages(OIV_Image_Render_mode renderMode)
    {
        for (auto& item : fImageEntries)
        {
            ImageEntry& entry = item.second;
            if (entry.renderable->GetImageRenderMode() == renderMode)
                DrawImage(entry);
        }
    }

    void OIVGLRenderer::DrawImage(ImageEntry& entry)
    {
        IRenderable& renderable = *entry.renderable;
        if (!renderable.GetVisible() || renderable.GetOpacity() <= 0.0)
            return;

        renderable.PreRender();
        if (renderable.GetIsImageDirty() || entry.texture == nullptr)
        {
            IMCodec::ImageSharedPtr image = renderable.GetImage();
            if (image == nullptr)
                return;

            if (image->GetTexelFormat() != IMCodec::TexelFormat::I_R8_G8_B8_A8)
                image = IMUtil::ImageUtil::Convert(image, IMCodec::TexelFormat::I_R8_G8_B8_A8);
            if (image == nullptr)
                throw std::runtime_error("Could not convert an image for OpenGL rendering");

            if (entry.texture == nullptr)
                entry.texture = std::make_unique<GLTexture>();

            entry.texture->SetRGBATexture(image->GetWidth(), image->GetHeight(), image->GetBuffer());
            entry.width  = image->GetWidth();
            entry.height = image->GetHeight();
            renderable.ClearImageDirty();
        }

        const OIV_Filter_type filter = renderable.GetFilterType() == FT_Lanczos3 ? fFilterType
                                                                                 : renderable.GetFilterType();
        entry.texture->SetFilter(filter == FT_None ? GL_NEAREST : GL_LINEAR);
        entry.texture->Bind();
        UpdateGpuParams(entry);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void OIVGLRenderer::DrawSelectionRect() const
    {
        if (fSelectionRect.p0.x == -1)
            return;

        const std::array<float, 4> selectionRect{
            static_cast<float>(fSelectionRect.p0.x),
            static_cast<float>(fSelectionRect.p0.y),
            static_cast<float>(fSelectionRect.p1.x),
            static_cast<float>(fSelectionRect.p1.y),
        };
        fSelectionProgram->Bind();
        fSelectionProgram->SetUniform2F("viewportSize", fViewportSize[0], fViewportSize[1]);
        fSelectionProgram->SetUniform4F("selectionRect", selectionRect.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    void OIVGLRenderer::UpdateViewportSize(int width, int height)
    {
#if defined(OIV_GL_WAYLAND)
        const bool firstWaylandResize = fViewportSize[0] == 0.0F && fViewportSize[1] == 0.0F && width > 0 && height > 0;
#endif
        fContext.Resize(width, height);
        glViewport(0, 0, width, height);
        fViewportSize = {static_cast<float>(width), static_cast<float>(height)};
#if defined(OIV_GL_WAYLAND)
        if (firstWaylandResize)
        {
            // Mesa acquires the initial 1x1 EGL buffer while creating the context. Retire it after resizing so the
            // first rendered frame uses a buffer matching the Wayland surface geometry.
            fContext.SwapBuffers();
        }
#endif
    }

    void OIVGLRenderer::UpdateGpuParams(const ImageEntry& entry) const
    {
        const LLUtils::PointF64 scale    = entry.renderable->GetScale();
        const LLUtils::PointF64 position = entry.renderable->GetPosition();

        fProgram->Bind();
        fProgram->SetUniform2F("viewportSize", fViewportSize[0], fViewportSize[1]);
        fProgram->SetUniform2F("imageSize", static_cast<float>(entry.width), static_cast<float>(entry.height));
        fProgram->SetUniform2F("imageScale", static_cast<float>(scale.x), static_cast<float>(scale.y));
        fProgram->SetUniform2F("imageOffset", static_cast<float>(position.x), static_cast<float>(position.y));
        fProgram->SetUniform4F("backgroundColor1", fBackgroundColors[0].data());
        fProgram->SetUniform4F("backgroundColor2", fBackgroundColors[1].data());
        fProgram->SetUniform4F("transparencyColor1", fTransparencyColors[0].data());
        fProgram->SetUniform4F("transparencyColor2", fTransparencyColors[1].data());
        fProgram->SetUniform1F("opacity", static_cast<float>(entry.renderable->GetOpacity()));
        fProgram->SetUniform1F("exposure", fExposure);
        fProgram->SetUniform1F("colorOffset", fOffset);
        fProgram->SetUniform1F("gamma", fGamma);
        fProgram->SetUniform1F("saturation", fSaturation);
        fProgram->SetUniform1I("imageRenderMode", entry.renderable->GetImageRenderMode());
        fProgram->SetUniform1I("showGrid", fShowGrid ? 1 : 0);
    }
}  // namespace OIV
