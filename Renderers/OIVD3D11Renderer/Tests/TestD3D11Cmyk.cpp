#if defined(OIV_TEST_D3D11_RENDERER)
    #include <catch2/catch_test_macros.hpp>
    #include <Image.h>
    #include "../Source/OIVD3DHelper.h"
    #include "../Source/D3D11/D3D11Texture.h"
    #include <array>
    #include <cstring>
    #include <type_traits>

TEST_CASE("D3D11 uploads native CMYK through the shared RGBA conversion", "[renderer][d3d11][CMYK]")
{
    // A hidden WARP window exercises the real upload without requiring a particular GPU.
    const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> window(
        CreateWindowExW(0, L"STATIC", L"CMYK upload test", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                        GetModuleHandleW(nullptr), nullptr),
        DestroyWindow);
    REQUIRE(window != nullptr);
    const auto device = std::make_shared<OIV::D3D11Device>();
    REQUIRE_NOTHROW(device->Create(window.get(), -1, nullptr, OIV::Acceleration::Software));
    auto item        = std::make_shared<IMCodec::ImageItem>();
    item->descriptor = {.width                   = 3,
                        .height                  = 2,
                        .rowPitchInBytes         = 17,
                        .texelFormatDecompressed = IMCodec::TexelFormat::I_C8_M8_Y8_K8};
    item->data.Allocate(34);
    REQUIRE(item->data.data() != nullptr);
    std::memset(item->data.data(), 0x5a, item->data.size());
    constexpr std::array<unsigned char, 4> ink = {0, 128, 255, 64};
    for (size_t row = 0; row < 2; ++row)
        for (size_t column = 0; column < 3; ++column)
            std::memcpy(item->data.data() + row * 17 + column * 4, ink.data(), ink.size());
    const auto image   = std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
    const auto texture = OIV::OIVD3DHelper::CreateTexture(device, image, false);
    REQUIRE(texture != nullptr);
    CHECK(texture->GetCreateParams().format == DXGI_FORMAT_R8G8B8A8_UNORM);
    texture->Use();

    // Read the bound resource through D3D11 without adding production accessors for the test.
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    device->GetContext()->PSGetShaderResources(0, 1, view.GetAddressOf());
    REQUIRE(view != nullptr);
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    view->GetResource(resource.GetAddressOf());
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gpuTexture;
    REQUIRE(SUCCEEDED(resource.As(&gpuTexture)));
    D3D11_TEXTURE2D_DESC description{};
    gpuTexture->GetDesc(&description);
    description.Usage          = D3D11_USAGE_STAGING;
    description.BindFlags      = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags      = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    REQUIRE(SUCCEEDED(device->GetdDevice()->CreateTexture2D(&description, nullptr, staging.GetAddressOf())));
    device->GetContext()->CopyResource(staging.Get(), gpuTexture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    REQUIRE(SUCCEEDED(device->GetContext()->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)));
    constexpr std::array<unsigned char, 4> expected = {191, 95, 0, 255};
    for (size_t row = 0; row < 2; ++row)
        for (size_t column = 0; column < 3; ++column)
            CHECK(std::memcmp(static_cast<const unsigned char*>(mapped.pData) + row * mapped.RowPitch + column * 4,
                              expected.data(), expected.size()) == 0);
    device->GetContext()->Unmap(staging.Get(), 0);
    CHECK(image->GetTexelFormat() == IMCodec::TexelFormat::I_C8_M8_Y8_K8);
    CHECK(std::memcmp(image->GetBuffer(), ink.data(), ink.size()) == 0);
}
#endif
