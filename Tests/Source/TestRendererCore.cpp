#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <RendererCore/Renderer.h>
#include <ImageProcessing/RawImage.h>
#include <OIVAppCore/ImageState.h>
#include <ImageProcessing/TextRasterizer.h>
#include "LabelManager.h"
#include <array>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    // Graphics double observes borrowed records and release without allocating GPU resources.
    class Backend final : public OIV::IRenderBackend
    {
      public:

        int* releaseSnapshot  = nullptr;
        int* destructionCount = nullptr;
        ~Backend() override
        {
            if (releaseSnapshot)
                *releaseSnapshot = releases;
            if (destructionCount)
                ++*destructionCount;
        }
        // Number of logical releases, including never-uploaded registrations.
        int releases = 0;
        // Last released renderer-local identity.
        uint64_t releasedId = 0;
        // Revision observed through the current frame.
        uint64_t revision = 0;
        // First identity observed in the current frame.
        uint64_t id = 0;
        // Number of borrowed common records in the current frame.
        size_t count = 0;
        // Owned pixel snapshot of the first record, without retaining borrowed record storage.
        IMCodec::ImageSharedPtr pixels;
        // Last registration identity, used to verify overlay creation order.
        uint64_t lastId = 0;
        // Whether the first record is visible in the prepared frame.
        bool visible = false;
        // Last record's content revision, observed without retaining a borrowed record.
        uint64_t lastRevision = 0;
        void Init(const OIV::RendererInitialization&) override {}
        OIV::ViewParameters view{};
        void SetViewParams(const OIV::ViewParameters& parameters) override { view = parameters; }
        void Render(const OIV::RenderFrame& frame) override
        {
            count = frame.images.size();
            if (!frame.images.empty())
            {
                id           = frame.images.begin()->first;
                revision     = frame.images.begin()->second.revision;
                pixels       = frame.images.begin()->second.pixels;
                visible      = frame.images.begin()->second.properties.visible;
                lastId       = frame.images.rbegin()->first;
                lastRevision = frame.images.rbegin()->second.revision;
            }
        }
        void ReleaseImageResources(uint64_t imageId) noexcept override
        {
            ++releases;
            releasedId = imageId;
        }
        void SetFilterLevel(OIV::FilterType) override {}
        void SetExposure(const OIV::ColorExposure&) override {}
        void SetSelectionRect(OIV::VisualSelectionRect) override {}
        void SetBackgroundColor(int, LLUtils::Color) override {}
    };

    IMCodec::ImageSharedPtr Pixels()
    {
        const std::array<std::byte, 16> bytes{};
        return OIV::CreateRawImage({bytes.data(), 2, 2, 8, IMCodec::TexelFormat::I_R8_G8_B8_A8}, {});
    }
    static_assert(!std::is_copy_constructible_v<OIV::RenderImage>);
    static_assert(std::is_nothrow_move_constructible_v<OIV::RenderImage>);
    static_assert(std::is_nothrow_move_assignable_v<OIV::RenderImage>);
    static_assert(!std::is_move_constructible_v<OIV::Renderer>);
}  // namespace

TEST_CASE("Common renderer owns registration identity and content revisions", "[renderer][core]")
{
    auto owner    = std::make_unique<Backend>();
    auto* backend = owner.get();
    OIV::Renderer renderer(std::move(owner));
    auto pixels = Pixels();
    {
        auto image = renderer.CreateImage(pixels);
        REQUIRE_NOTHROW(renderer.Redraw());
        const auto id       = backend->id;
        const auto revision = backend->revision;
        image.SetImage(pixels);
        image.SetProperties({.position = {10, 20}, .opacity = 0.5});
        renderer.Redraw();
        CHECK(backend->revision == revision);
        auto moved = std::move(image);
        moved      = std::move(moved);
        renderer.Redraw();
        CHECK(backend->id == id);
        CHECK(backend->releases == 0);
        moved.SetImage({});
        renderer.Redraw();
        CHECK(backend->revision == revision + 1);
        moved.SetImage(Pixels());
        renderer.Redraw();
        CHECK(backend->revision == revision + 2);
    }
    CHECK(backend->releases == 1);
    renderer.Redraw();
    CHECK(backend->count == 0);
}

TEST_CASE("Move assignment releases its destination and renderers remain independent", "[renderer][core]")
{
    int firstReleases      = 0;
    auto firstOwner        = std::make_unique<Backend>();
    auto* first            = firstOwner.get();
    first->releaseSnapshot = &firstReleases;
    auto secondOwner       = std::make_unique<Backend>();
    auto* second           = secondOwner.get();
    OIV::Renderer a(std::move(firstOwner)), b(std::move(secondOwner));
    auto pixels = Pixels();
    {
        auto source      = a.CreateImage(pixels);
        auto destination = a.CreateImage({});
        auto independent = b.CreateImage(pixels);
        destination      = std::move(source);
        CHECK(first->releases == 1);
        destination.SetImage({});
        a.Redraw();
        b.Redraw();
        CHECK(first->count == 1);
        CHECK(second->count == 1);
        CHECK(first->revision == 2);
        CHECK(second->revision == 1);
        a.Shutdown();
    }
    CHECK(firstReleases == 1);
    CHECK(second->releases == 1);
}

TEST_CASE("Text producer prepares and clears CPU pixels without a renderer", "[image][cpu][text]")
{
    FreeType::FreeTypeConnector connector;
    OIV::TextRasterizer text(&connector);
    CHECK(text.GetBitmap() == nullptr);
    CHECK_FALSE(text.IsBitmapDirty());
    CHECK(text.GetMetrics().pixelSize == LLUtils::PointI32::Zero);
    text.SetFontPath(LLUTILS_TEXT("Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf"));
    text.SetFontSize(14);
    text.SetText(LLUTILS_TEXT("OIV"));
    const auto pixels = text.GetBitmap();
    REQUIRE(pixels);
    CHECK(text.GetBitmap() == pixels);
    text.SetText({});
    CHECK(text.GetBitmap() == nullptr);
    CHECK_FALSE(text.IsBitmapDirty());
    text.SetText(LLUTILS_TEXT("OIV"));
    REQUIRE(text.GetBitmap());
}

TEST_CASE("ImageState processes cached pixels without presentation ownership", "[image][cpu][pipeline]")
{
    OIV::ImageState state;
    auto pixels = Pixels();
    state.SetOpenedImage(OIV::ViewerImage{.pixels = pixels});
    const auto generation = state.GetSourceGeneration();
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == nullptr);
    state.Refresh(OIV::ImageProcessingStage::GeometricTransform);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform) == pixels);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == nullptr);
    state.Refresh();
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == pixels);
    state.SetColorCountPending();
    state.CompleteColorCount(4);
    state.SetScale({0.5, 0.5});
    state.SetOffset({10, 20});
    state.SetResample(true);
    state.Refresh();
    CHECK(state.GetSourceGeneration() == generation);
    CHECK(state.GetNumUniqueColors() == 4);
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::Resampled));
    CHECK(state.GetVisibleImage()->GetWidth() == 1);
    CHECK(state.GetVisibleImage()->GetHeight() == 1);
    state.SetResample(false);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::Resampled) == nullptr);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == pixels);
    state.SetOpenedImage(OIV::ViewerImage{.pixels = pixels});
    CHECK(state.GetSourceGeneration() != generation);
    CHECK(state.GetColorCountState() == OIV::ColorCountState::NotRequested);
    state.ClearAll();
    CHECK(state.GetOpenedImage() == nullptr);
    CHECK(state.GetSelectedSource() == nullptr);
}

TEST_CASE("Visible text preparation preserves pixels, empty output and retry work", "[renderer][text][preparation]")
{
    auto owner    = std::make_unique<Backend>();
    auto* backend = owner.get();
    OIV::Renderer renderer(std::move(owner));
    FreeType::FreeTypeConnector font;
    OIV::TextLabel label(&font);
    const auto fontPath = LLUTILS_TEXT("Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf");
    label.SetFontPath(fontPath);
    label.SetFontSize(14);
    label.SetText(LLUTILS_TEXT("OIV"));
    label.SetVisible(false);
    REQUIRE(label.GetMetrics().pixelSize.x > 0);
    label.Prepare(renderer);
    renderer.Redraw();
    CHECK(backend->pixels == nullptr);
    CHECK_FALSE(backend->visible);
    CHECK(label.IsDirty());
    const auto id = backend->id;
    label.SetVisible(true);
    label.Prepare(renderer);
    renderer.Redraw();
    REQUIRE(backend->pixels);
    const auto bitmap   = backend->pixels;
    const auto revision = backend->revision;
    label.SetPosition({10, 20});
    label.SetOpacity(0.5);
    label.Prepare(renderer);
    renderer.Redraw();
    CHECK(backend->pixels == bitmap);
    CHECK(backend->revision == revision);
    label.SetText({});
    label.Prepare(renderer);
    renderer.Redraw();
    CHECK(backend->pixels == nullptr);
    CHECK(backend->id == id);
    CHECK_FALSE(label.IsDirty());
    label.SetText(LLUTILS_TEXT("OIV"));
    label.SetFontPath({});
    REQUIRE_THROWS(label.Prepare(renderer));
    CHECK(label.IsDirty());
    label.SetFontPath(fontPath);
    REQUIRE_NOTHROW(label.Prepare(renderer));
    renderer.Redraw();
    REQUIRE(backend->pixels);
    CHECK(backend->id == id);
}

TEST_CASE("Label manager registers pre-initialization labels in creation order", "[renderer][text][order]")
{
    auto owner    = std::make_unique<Backend>();
    auto* backend = owner.get();
    OIV::Renderer renderer(std::move(owner));
    FreeType::FreeTypeConnector font;
    OIV::LabelManager labels(&font, std::filesystem::path("Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf"));
    auto* first = labels.GetOrCreateTextLabel("z-first");
    auto* last  = labels.GetOrCreateTextLabel("a-last");
    first->SetFontPath(LLUTILS_TEXT("Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf"));
    last->SetFontPath(LLUTILS_TEXT("Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf"));
    first->SetVisible(false);
    last->SetVisible(false);
    labels.InitializeRenderer(renderer);
    auto anchor = renderer.CreateImage({});
    labels.Prepare(renderer);
    renderer.Redraw();
    CHECK(backend->count == 3);
    const auto firstId  = backend->id;
    const auto anchorId = backend->lastId;
    const auto lastId   = firstId + 1;
    last->SetVisible(true);
    last->SetText(LLUTILS_TEXT("Last"));
    labels.Prepare(renderer);
    renderer.Redraw();
    CHECK(backend->id == firstId);
    CHECK(backend->lastId == anchorId);
    CHECK(backend->revision == 1);
    labels.Remove("z-first");
    renderer.Redraw();
    CHECK(backend->id == lastId);
    labels.RemoveAll();
    CHECK(backend->releases == 2);
    CHECK(anchorId > lastId);
}

TEST_CASE("Raw creation copies padded rows and geometric refresh invalidates later results", "[image][cpu][pipeline]")
{
    std::array<std::byte, 36> source{};
    source[0]   = std::byte{0x42};
    auto pixels = OIV::CreateRawImage({source.data(), 2, 3, 12, IMCodec::TexelFormat::I_R8_G8_B8_A8}, {});
    CHECK(pixels->GetRowPitchInBytes() == 12);
    source[0] = std::byte{};
    CHECK(pixels->GetBuffer()[0] == std::byte{0x42});
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = pixels});
    state.Refresh();
    state.Transform(IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::None);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform) == nullptr);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == nullptr);
    state.Refresh(OIV::ImageProcessingStage::GeometricTransform);
    CHECK(state.GetSelectedSource() == pixels);
    CHECK(state.GetOpenedImage()->pixels == pixels);
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform));
    CHECK(state.GetVisibleSize() == LLUtils::PointF64{3, 2});
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == nullptr);
}

TEST_CASE("CPU resampling respects padded byte rows without normalizing the source", "[image][cpu][pipeline]")
{
    const uint32_t rowPitch = GENERATE(16u, 17u, 20u, 25u);
    // Padding deliberately differs from every channel so accidental sampling is visible.
    std::vector<std::byte> source(rowPitch * 4, std::byte{0xEF});
    std::array<std::byte, 64> packed{};
    for (size_t y = 0; y < 4; ++y)
        for (size_t x = 0; x < 4; ++x)
            for (size_t channel = 0; channel < 4; ++channel)
            {
                const auto value                       = std::byte(10 + y * 20 + x * 4 + channel);
                source[y * rowPitch + x * 4 + channel] = value;
                packed[y * 16 + x * 4 + channel]       = value;
            }
    const auto pixels = OIV::CreateRawImage({source.data(), 4, 4, rowPitch, IMCodec::TexelFormat::I_R8_G8_B8_A8}, {});
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = pixels});
    state.SetScale({0.5, 0.5});
    state.SetResample(true);
    state.Refresh();
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == pixels);
    const auto result = state.GetRetainedImage(OIV::ImageProcessingStage::Resampled);
    REQUIRE(result);
    REQUIRE(result->GetWidth() == 2);
    REQUIRE(result->GetHeight() == 2);

    OIV::Resampler resampler;
    const auto reference = resampler.Resample(
        OIV::CreateRawImage({packed.data(), 4, 4, 16, IMCodec::TexelFormat::I_R8_G8_B8_A8}, {}), {2, 2});
    for (size_t y = 0; y < 2; ++y)
        for (size_t x = 0; x < 2; ++x)
            for (size_t channel = 0; channel < 4; ++channel)
            {
                CHECK(result->GetBufferAt(x, y)[channel] == reference->GetBufferAt(x, y)[channel]);
                // Each 2x2 neighborhood averages independently across all four channels.
                CHECK(result->GetBufferAt(x, y)[channel] == std::byte(22 + y * 40 + x * 8 + channel));
            }
}

TEST_CASE("View parameters reach each backend without narrowing or shared state", "[renderer][core][viewport]")
{
    auto firstOwner  = std::make_unique<Backend>();
    auto secondOwner = std::make_unique<Backend>();
    auto* first      = firstOwner.get();
    auto* second     = secondOwner.get();
    OIV::Renderer a(std::move(firstOwner)), b(std::move(secondOwner));
    for (const auto extent : {LLUtils::PointI32{}, LLUtils::PointI32{70000, 80000}, LLUtils::PointI32{},
                              LLUtils::PointI32{640, 480}, LLUtils::PointI32{640, 480}})
    {
        a.SetViewParams({.uViewportSize = extent, .showGrid = true});
        CHECK(first->view.uViewportSize == extent);
        CHECK(first->view.showGrid);
        CHECK(second->view.uViewportSize == LLUtils::PointI32{});
        CHECK_FALSE(second->view.showGrid);
    }
}

TEST_CASE("Backend ownership ends exactly once at renderer shutdown", "[renderer][core][lifetime]")
{
    int destroyed           = 0;
    auto owner              = std::make_unique<Backend>();
    owner->destructionCount = &destroyed;
    OIV::Renderer renderer(std::move(owner));
    auto image = std::make_optional(renderer.CreateImage({}));
    renderer.Shutdown();
    CHECK(destroyed == 1);
    REQUIRE_NOTHROW(renderer.Shutdown());
    CHECK(destroyed == 1);
    REQUIRE_NOTHROW(image.reset());
    CHECK(destroyed == 1);
}

TEST_CASE("Viewer label scales are independent", "[renderer][wlx]")
{
    FreeType::FreeTypeConnector font;
    const std::filesystem::path path("Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf");
    OIV::LabelManager first(&font, path);
    OIV::LabelManager second(&font, path);
    auto* left  = first.GetOrCreateTextLabel("scale");
    auto* right = second.GetOrCreateTextLabel("scale");
    left->SetText(LLUTILS_TEXT("Independent viewer"));
    right->SetText(LLUTILS_TEXT("Independent viewer"));
    const auto original = right->GetMetrics().pixelSize;
    CHECK(left->GetMetrics().pixelSize == original);
    first.SetContentScale({2.0, 2.0});
    CHECK(left->GetMetrics().pixelSize.x > original.x);
    CHECK(right->GetMetrics().pixelSize == original);
}
