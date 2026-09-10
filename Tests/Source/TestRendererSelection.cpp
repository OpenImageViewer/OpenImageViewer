#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "RendererSelection.h"
#include "NullRenderer.h"
#include "../../OIVLib/Renderers/OIVGLRenderer/GLAcceleration.h"
#include <set>
#include <stdexcept>

namespace
{
    using namespace OIV;
    struct Backend
    {
        std::vector<RendererAdapter> adapters;
        std::set<int> failures;
        bool discoveryFails = false;
        Acceleration gl     = Acceleration::Unknown;
        int discoveries     = 0;
        std::vector<int> attempts;
    };
    struct Fixture
    {
        std::array<Backend, 3> backends;
        int live = 0;
        Fixture();
        ~Fixture();
    };
    Fixture* fixture;
    Fixture::Fixture()
    {
        fixture = this;
    }
    Fixture::~Fixture()
    {
        CHECK(live == 0);
        fixture = nullptr;
    }
    class FakeRenderer : public NullRenderer
    {
      public:

        explicit FakeRenderer(size_t api) : fApi(api)
        {
            // Factories must never run while failed, probing, or deferred instances are alive.
            CHECK(fixture->live == 0);
            ++fixture->live;
        }
        ~FakeRenderer() override { --fixture->live; }
        std::vector<RendererAdapter> EnumerateAdapters() override
        {
            auto& backend = fixture->backends[fApi];
            ++backend.discoveries;
            if (backend.discoveryFails)
                throw std::runtime_error("runtime unavailable");
            return backend.adapters;
        }
        int Init(const OIV_RendererInitializationParams& params) override
        {
            CHECK(params.adapterName == nullptr);
            auto& backend = fixture->backends[fApi];
            backend.attempts.push_back(params.gpuIndex);
            if (backend.failures.contains(params.gpuIndex))
                throw std::runtime_error("unsupported device or initialization failure");
            fAcceleration = backend.gl;
            if (fApi != 2)
            {
                const auto adapter = std::ranges::find(backend.adapters, params.gpuIndex, &RendererAdapter::index);
                REQUIRE(adapter != backend.adapters.end());
                fAcceleration = adapter->acceleration;
            }
            return 0;
        }
        Acceleration GetAcceleration() const override { return fAcceleration; }
        const char* GetBackendName() const override { return std::array{"Vulkan", "D3D11", "GL"}[fApi]; }

      private:

        size_t fApi;
        Acceleration fAcceleration = Acceleration::Unknown;
    };
    constexpr std::array<RendererBackend, 3> Backends{{
        {{RendererType::Vulkan, "Vulkan", true},
         +[]() -> IRendererSharedPtr { return std::make_shared<FakeRenderer>(0); }},
        {{RendererType::D3D11, "D3D11", true},
         +[]() -> IRendererSharedPtr { return std::make_shared<FakeRenderer>(1); }},
        {{RendererType::OpenGL, "GL", false},
         +[]() -> IRendererSharedPtr { return std::make_shared<FakeRenderer>(2); }},
    }};
    RendererAdapter Adapter(int index, Acceleration acceleration, std::string name = "GPU", uint32_t vendor = 0)
    {
        return {.index = index, .name = std::move(name), .vendorId = vendor, .acceleration = acceleration};
    }
}  // namespace

TEST_CASE("Startup exhausts hardware before software and discovers lazily", "[renderer][policy]")
{
    Fixture state;
    const auto hardwareApi     = GENERATE(0, 1, 2);
    state.backends[0].adapters = {Adapter(0, hardwareApi == 0 ? Acceleration::Hardware : Acceleration::Software)};
    state.backends[1].adapters = {Adapter(0, hardwareApi == 1 ? Acceleration::Hardware : Acceleration::Software)};
    state.backends[2].gl       = Acceleration::Hardware;
    const auto selected        = SelectRenderer(Backends, {}, {});
    CHECK(std::string(selected->GetBackendName()) == Backends[hardwareApi].info.name);
    for (int api = 0; api < hardwareApi; ++api)
        CHECK(state.backends[api].attempts.empty());
    for (int api = hardwareApi + 1; api < 3; ++api)
    {
        CHECK(state.backends[api].discoveries == 0);
        CHECK(state.backends[api].attempts.empty());
    }
}

TEST_CASE("Unknown GL precedes software but never confirmed hardware", "[renderer][policy]")
{
    Fixture state;
    const bool hardware        = GENERATE(false, true);
    state.backends[0].adapters = {Adapter(0, Acceleration::Software)};
    state.backends[1].adapters = {Adapter(0, hardware ? Acceleration::Hardware : Acceleration::Software)};
    state.backends[2].gl       = Acceleration::Unknown;
    const auto selected        = SelectRenderer(Backends, {}, {});
    CHECK(std::string(selected->GetBackendName()) == (hardware ? "D3D11" : "GL"));
    CHECK(state.backends[0].attempts.empty());
    CHECK(state.backends[2].attempts.size() == (hardware ? 0 : 2));
}

TEST_CASE("Software tier retains API order after failed hardware and unknown", "[renderer][policy]")
{
    Fixture state;
    state.backends[0].adapters = {Adapter(0, Acceleration::Hardware), Adapter(1, Acceleration::Software)};
    state.backends[0].failures = {0};
    state.backends[1].adapters = {Adapter(0, Acceleration::Unknown)};
    state.backends[1].failures = {0};
    state.backends[2].gl       = Acceleration::Software;
    const auto selected        = SelectRenderer(Backends, {}, {});
    CHECK(std::string(selected->GetBackendName()) == "Vulkan");
    CHECK(state.backends[0].attempts == std::vector<int>{0, 1});
    CHECK(state.backends[1].attempts == std::vector<int>{0});
}

TEST_CASE("Explicit API permits its software but forbids another API", "[renderer][policy]")
{
    Fixture state;
    state.backends[0].adapters = {Adapter(0, Acceleration::Hardware)};
    state.backends[1].adapters = {Adapter(0, Acceleration::Hardware), Adapter(1, Acceleration::Software)};
    state.backends[1].failures = {0};
    const auto selected        = SelectRenderer(Backends, {.renderer = RendererType::D3D11}, {});
    CHECK(std::string(selected->GetBackendName()) == "D3D11");
    CHECK(selected->GetAcceleration() == Acceleration::Software);
    CHECK(state.backends[0].discoveries == 0);
    CHECK(state.backends[2].attempts.empty());
}

TEST_CASE("Name constraints survive fallback and select the first usable match", "[renderer][policy]")
{
    Fixture state;
    state.backends[0].adapters              = {Adapter(0, Acceleration::Hardware, "GeForce", 0x10de)};
    state.backends[1].adapters              = {Adapter(0, Acceleration::Hardware, "Radeon A", 0x1002),
                                               Adapter(1, Acceleration::Hardware, "Radeon B", 0x1002),
                                               Adapter(2, Acceleration::Hardware, "Radeon C", 0x1002)};
    state.backends[1].adapters[2].preferred = true;
    state.backends[1].failures              = {0};
    const auto selected                     = SelectRenderer(Backends, {.adapterName = "  aMd  "}, {});
    CHECK(std::string(selected->GetBackendName()) == "D3D11");
    CHECK(state.backends[0].attempts.empty());
    CHECK(state.backends[1].attempts == std::vector<int>{0, 1});
    CHECK(state.backends[2].attempts.empty());
}

TEST_CASE("Index overrides conflicting name and disables adapter and API fallback", "[renderer][policy]")
{
    Fixture state;
    const bool failure         = GENERATE(false, true);
    state.backends[0].adapters = {Adapter(0, Acceleration::Hardware, "Intel", 0x8086),
                                  Adapter(1, Acceleration::Hardware, "Nvidia", 0x10de)};
    if (failure)
        state.backends[0].failures = {0};
    const RendererOptions options{.renderer = RendererType::Vulkan, .adapterName = "Nvidia", .adapterIndex = 0};
    if (failure)
        CHECK_THROWS_WITH(SelectRenderer(Backends, options, {}),
                          Catch::Matchers::ContainsSubstring("initialization failure"));
    else
    {
        const auto selected = SelectRenderer(Backends, options, {});
        CHECK(std::string(selected->GetBackendName()) == "Vulkan");
    }
    CHECK(state.backends[0].attempts == std::vector<int>{0});
    CHECK(state.backends[1].discoveries == 0);
    CHECK(state.backends[2].attempts.empty());
    CHECK_THROWS(
        SelectRenderer(Backends, {.renderer = RendererType::Vulkan, .adapterName = "Intel", .adapterIndex = 99}, {}));
}

TEST_CASE("Automatic GPU preference precedes remaining eligible hardware", "[renderer][policy]")
{
    Fixture state;
    state.backends[0].adapters              = {Adapter(0, Acceleration::Hardware), Adapter(1, Acceleration::Hardware),
                                               Adapter(2, Acceleration::Software)};
    state.backends[0].adapters[1].preferred = true;
    state.backends[0].failures              = {1};
    const auto selected                     = SelectRenderer(Backends, {}, {});
    CHECK(state.backends[0].attempts == std::vector<int>{1, 0});
}

TEST_CASE("Missing runtime falls back automatically and explicit Vulkan fails", "[renderer][policy]")
{
    Fixture state;
    state.backends[0].discoveryFails = true;
    state.backends[1].adapters       = {Adapter(0, Acceleration::Hardware)};
    {
        const auto selected = SelectRenderer(Backends, {}, {});
        CHECK(std::string(selected->GetBackendName()) == "D3D11");
    }
    CHECK_THROWS_WITH(SelectRenderer(Backends, {.renderer = RendererType::Vulkan}, {}),
                      Catch::Matchers::ContainsSubstring("runtime unavailable"));
    if (GetDefaultRenderer() == RendererType::Vulkan)
        CHECK_THROWS(SelectRenderer(Backends, {.adapterIndex = 0}, {}));
}

TEST_CASE("Adapter matching uses vendor IDs and preserves UTF-8", "[renderer][policy]")
{
    CHECK(detail::AdapterNameMatches("nViDiA", "GeForce RTX", 0x10de));
    CHECK(detail::AdapterNameMatches("aMD", "Radeon", 0x1002));
    CHECK(detail::AdapterNameMatches("INTEL", "Arc", 0x8086));
    CHECK_FALSE(detail::AdapterNameMatches("AMD", "AMD in another vendor's display name", 0x10de));
    CHECK(detail::AdapterNameMatches("rTx 2000", "NVIDIA RTX 2000 Ada"));
    CHECK(detail::AdapterNameMatches("GPU \xc3\xa9", "Test gpu \xc3\xa9 device"));
    CHECK_FALSE(detail::AdapterNameMatches("GPU \xc3\x89", "GPU \xc3\xa9"));
    CHECK_FALSE(detail::AdapterNameMatches(" \t", "GPU"));
    CHECK_FALSE(detail::AdapterNameMatches("missing", "GPU"));
}

TEST_CASE("GL classifies recognized software before driver vendor identities", "[renderer][policy]")
{
    CHECK(ClassifyGLAcceleration("Intel", "llvmpipe") == Acceleration::Software);
    CHECK(ClassifyGLAcceleration("Mesa", "softpipe") == Acceleration::Software);
    CHECK(ClassifyGLAcceleration("Microsoft Corporation", "GDI Generic") == Acceleration::Software);
    CHECK(ClassifyGLAcceleration("NVIDIA Corporation", "GeForce RTX") == Acceleration::Hardware);
    CHECK(ClassifyGLAcceleration("AMD", "Radeon") == Acceleration::Hardware);
    CHECK(ClassifyGLAcceleration("Mesa", "Unclassified driver") == Acceleration::Unknown);
}

TEST_CASE("An explicitly selected GL context initializes once regardless of acceleration", "[renderer][policy]")
{
    Fixture state;
    state.backends[2].gl = GENERATE(Acceleration::Hardware, Acceleration::Unknown, Acceleration::Software);
    const auto renderer  = SelectRenderer(Backends, {.renderer = RendererType::OpenGL}, {});
    CHECK(renderer->GetAcceleration() == state.backends[2].gl);
    CHECK(state.backends[2].attempts.size() == 1);
    CHECK(state.backends[0].discoveries == 0);
    CHECK(state.backends[1].discoveries == 0);
}

TEST_CASE("Exhaustion reports every failed eligible API and retains name constraints", "[renderer][policy]")
{
    Fixture state;
    state.backends[0].adapters = {Adapter(0, Acceleration::Hardware)};
    state.backends[0].failures = {0};
    state.backends[1].adapters = {Adapter(0, Acceleration::Software)};
    state.backends[1].failures = {0};
    state.backends[2].failures = {-1};
    CHECK_THROWS_WITH(SelectRenderer(Backends, {}, {}), Catch::Matchers::ContainsSubstring("Vulkan adapter 0") &&
                                                            Catch::Matchers::ContainsSubstring("D3D11 adapter 0") &&
                                                            Catch::Matchers::ContainsSubstring("GL adapter -1"));
    CHECK_THROWS_WITH(SelectRenderer(Backends, {.adapterName = "missing"}, {}),
                      Catch::Matchers::ContainsSubstring("no adapters satisfy"));
    CHECK(state.backends[2].attempts.size() == 1);
}
