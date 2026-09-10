#pragma once
#include <windows.h>
#include <d3d11_2.h>
#include <dxgi1_4.h>
#include "D3D11Common.h"
#include "D3D11Error.h"
#include <Interfaces/RendererOptions.h>
#include <LLUtils/StringUtility.h>
#include <array>
#include <stdexcept>
#include <vector>

namespace OIV
{
    class D3D11Device
    {
      public:

        ID3D11DeviceContext* GetContext() const { return fD3dContext.Get(); }
        ID3D11Device* GetdDevice() const { return fD3dDevice.Get(); }
        int GetSelectedGPUIndex() const { return fGpuIndex; }
        Acceleration GetAcceleration() const { return fAcceleration; }
        IDXGIAdapter* GetAdapter() const { return fD3dAdapter.Get(); }
        IDXGISwapChain* GetSwapChain() const { return fD3dSwapChain.Get(); }
        DXGI_ADAPTER_DESC GetAdapterDesc() const
        {
            DXGI_ADAPTER_DESC desc{};
            if (fD3dAdapter)
                fD3dAdapter->GetDesc(&desc);
            return desc;
        }
        LARGE_INTEGER GetDriverVersion() const
        {
            LARGE_INTEGER version{};
            if (fD3dAdapter && FAILED(fD3dAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version)))
                version = {};
            return version;
        }
        static std::vector<RendererAdapter> EnumerateAdapters()
        {
            ComPtr<IDXGIFactory1> factory;
            D3D11Error::HandleDeviceError(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())),
                                          "Could not enumerate D3D11 adapters");
            std::vector<RendererAdapter> adapters;
            bool softwareFound = false;
            for (UINT index = 0;; ++index)
            {
                ComPtr<IDXGIAdapter1> adapter;
                const auto status = factory->EnumAdapters1(index, adapter.GetAddressOf());
                if (status == DXGI_ERROR_NOT_FOUND)
                    break;
                D3D11Error::HandleDeviceError(status, "Could not enumerate D3D11 adapters");
                DXGI_ADAPTER_DESC1 desc{};
                D3D11Error::HandleDeviceError(adapter->GetDesc1(&desc), "Could not read D3D11 adapter properties");
                const bool software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
                softwareFound |= software;
                adapters.push_back({static_cast<int>(index),
                                    LLUtils::StringUtility::ConvertString<std::string>(desc.Description), desc.VendorId,
                                    software ? Acceleration::Software : Acceleration::Hardware, index == 0});
            }
            // Older DXGI versions may omit WARP from indexed enumeration. It remains an explicit
            // software candidate, with no invented API index and no implicit hardware fallback.
            if (!softwareFound)
                adapters.push_back({-1, "Microsoft WARP", 0x1414, Acceleration::Software});
            return adapters;
        }
        void Create(HWND hwnd, int adapterIndex, const char* adapterName, std::optional<Acceleration> acceleration)
        {
            static constexpr std::array requestedLevels{D3D_FEATURE_LEVEL_11_0};
            constexpr UINT createFlags = D3D11_CREATE_DEVICE_SINGLETHREADED
#ifdef _DEBUG
                                         | D3D11_CREATE_DEVICE_DEBUG
#endif
                ;
            ComPtr<IDXGIFactory1> factory;
            D3D11Error::HandleDeviceError(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())),
                                          "Could not create DXGI factory");
            ComPtr<IDXGIAdapter1> selected;
            if (adapterIndex >= 0)
            {
                if (factory->EnumAdapters1(static_cast<UINT>(adapterIndex), selected.GetAddressOf()) ==
                    DXGI_ERROR_NOT_FOUND)
                    throw std::invalid_argument("--adapter_index is outside the D3D11 adapter list");
                if (!selected)
                    throw std::runtime_error("Could not enumerate the selected D3D11 adapter");
                fGpuIndex = adapterIndex;
            }
            else if (adapterName)
            {
                // Direct backend callers retain name selection; the viewer has already resolved
                // and filtered names through the shared policy before calling this method.
                for (const auto& adapter : EnumerateAdapters())
                    if (detail::AdapterNameMatches(adapterName, adapter.name, adapter.vendorId))
                    {
                        adapterIndex = adapter.index;
                        acceleration = adapter.acceleration;
                        if (adapterIndex >= 0)
                            D3D11Error::HandleDeviceError(factory->EnumAdapters1(static_cast<UINT>(adapterIndex),
                                                                                 selected.GetAddressOf()),
                                                          "Could not select D3D11 adapter");
                        fGpuIndex = adapterIndex;
                        break;
                    }
                if (!selected && acceleration != Acceleration::Software)
                    throw std::invalid_argument("Requested D3D11 adapter was not found");
            }
            const bool warp = !selected && acceleration == Acceleration::Software;
            auto status     = D3D11CreateDevice(selected.Get(),
                                                selected ? D3D_DRIVER_TYPE_UNKNOWN
                                                : warp   ? D3D_DRIVER_TYPE_WARP
                                                         : D3D_DRIVER_TYPE_HARDWARE,
                                                nullptr, createFlags, requestedLevels.data(),
                                                static_cast<UINT>(requestedLevels.size()), D3D11_SDK_VERSION,
                                                fD3dDevice.GetAddressOf(), nullptr, fD3dContext.GetAddressOf());
            // Only direct, unconstrained backend calls retain their local WARP fallback. The viewer
            // requests exact candidates so WARP cannot precede hardware in another API.
            if (FAILED(status) && !selected && !acceleration && !adapterName)
            {
                fD3dContext.Reset();
                fD3dDevice.Reset();
                status = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags, requestedLevels.data(),
                                           static_cast<UINT>(requestedLevels.size()), D3D11_SDK_VERSION,
                                           fD3dDevice.GetAddressOf(), nullptr, fD3dContext.GetAddressOf());
                fAcceleration = Acceleration::Software;
            }
            D3D11Error::HandleDeviceError(status, "The requested adapter cannot create a Direct3D 11 device");
            ComPtr<IDXGIDevice> dxgiDevice;
            D3D11Error::HandleDeviceError(fD3dDevice.As(&dxgiDevice), "Could not query DXGI device");
            D3D11Error::HandleDeviceError(dxgiDevice->GetAdapter(fD3dAdapter.GetAddressOf()),
                                          "Could not query selected adapter");
            ComPtr<IDXGIAdapter1> adapter1;
            D3D11Error::HandleDeviceError(fD3dAdapter.As(&adapter1), "Could not query selected adapter properties");
            DXGI_ADAPTER_DESC1 desc{};
            D3D11Error::HandleDeviceError(adapter1->GetDesc1(&desc), "Could not read selected adapter properties");
            fAcceleration = warp || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? Acceleration::Software
                                                                              : Acceleration::Hardware;
            if (fGpuIndex < 0)
            {
                for (UINT index = 0;; ++index)
                {
                    ComPtr<IDXGIAdapter1> candidate;
                    if (factory->EnumAdapters1(index, candidate.GetAddressOf()) != S_OK)
                        break;
                    DXGI_ADAPTER_DESC1 candidateDesc{};
                    if (SUCCEEDED(candidate->GetDesc1(&candidateDesc)) &&
                        candidateDesc.AdapterLuid.HighPart == desc.AdapterLuid.HighPart &&
                        candidateDesc.AdapterLuid.LowPart == desc.AdapterLuid.LowPart)
                    {
                        fGpuIndex = static_cast<int>(index);
                        break;
                    }
                }
            }
            ComPtr<IDXGIFactory2> swapFactory;
            D3D11Error::HandleDeviceError(fD3dAdapter->GetParent(IID_PPV_ARGS(swapFactory.GetAddressOf())),
                                          "DXGI 1.2 is required");
            ComPtr<IDXGIFactory4> factory4;
            const bool modern = SUCCEEDED(swapFactory.As(&factory4));
            const DXGI_SWAP_CHAIN_DESC1 swap{
                .Width       = 1280,
                .Height      = 800,
                .Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
                .Stereo      = FALSE,
                .SampleDesc  = {1, 0},
                .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                .BufferCount = 2,
                .Scaling     = modern ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH,
                .SwapEffect  = modern ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
                .AlphaMode   = DXGI_ALPHA_MODE_IGNORE,
            };
            D3D11Error::HandleDeviceError(swapFactory->CreateSwapChainForHwnd(fD3dDevice.Get(), hwnd, &swap, nullptr,
                                                                              nullptr, fD3dSwapChain.GetAddressOf()),
                                          "Could not create D3D11 swap chain");
            swapFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
            OIV_D3D_SET_OBJECT_NAME(fD3dDevice, "D3D11 device");
            OIV_D3D_SET_OBJECT_NAME(fD3dSwapChain, "D3D11 swap chain");
            OIV_D3D_SET_OBJECT_NAME(fD3dContext, "D3D11 context");
        }

      private:

        int fGpuIndex              = -1;
        Acceleration fAcceleration = Acceleration::Unknown;
        ComPtr<IDXGISwapChain1> fD3dSwapChain;
        ComPtr<ID3D11DeviceContext> fD3dContext;
        ComPtr<ID3D11Device> fD3dDevice;
        ComPtr<IDXGIAdapter> fD3dAdapter;
    };
}  // namespace OIV
