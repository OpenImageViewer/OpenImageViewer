#pragma once
#include <windows.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include "D3D11Common.h"
#include "D3D11Error.h"

namespace OIV
{
    class D3D11Device
    {
    public:
        ID3D11DeviceContext* GetContext()  const
        {
            return fD3dContext.Get();
        }

        ID3D11Device* GetdDevice()  const
        {
            return fD3dDevice.Get();
        }

        IDXGISwapChain* GetSwapChain()  const
        {
            return fD3dSwapChain.Get();
        }

        void Create(HWND hwnd)
        {
            fHWND = hwnd;
            D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
            D3D_FEATURE_LEVEL obtainedLevel;


            DXGI_SWAP_CHAIN_DESC scd = { 0 };

            scd.BufferCount = 1;
            scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
            scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
            scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

            scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            scd.OutputWindow = fHWND;
            scd.SampleDesc.Count = 1;
            scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            scd.Windowed = true;
            scd.BufferDesc.Width = 1280;
            scd.BufferDesc.Height = 800;
            scd.BufferDesc.RefreshRate.Numerator = 0;
            scd.BufferDesc.RefreshRate.Denominator = 1;

            UINT createFlags = 0;
            createFlags |= D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef _DEBUG
            createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

                //D3D11CreateDevice(
                //    nullptr
                //    , D3D_DRIVER_TYPE_HARDWARE
                //    , nullptr
                //    , createFlags
                //    , requestedLevels
                //    , sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL)
                //    , D3D11_SDK_VERSION
                //    , fD3dDevice.GetAddressOf()
                //    , &obtainedLevel
                //    , fD3dContext.GetAddressOf()
                //);

                //ComPtr<IDXGIDevice> dxgiDevice;
                //ComPtr<IDXGIAdapter> dxgiAdapter;
                //ComPtr<IDXGIFactory>  dxgiFactory;
                //D3D11Error::HandleDeviceError(fD3dDevice.As(&dxgiDevice)
                //    , "Could not query for a device");
                //D3D11Error::HandleDeviceError(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)dxgiAdapter.GetAddressOf())
                //    , "Could not query for a DXGI adapter");
                //D3D11Error::HandleDeviceError(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void **)dxgiFactory.GetAddressOf())
                //    , "Could not query for a DXGI factory");

                //D3D11Error::HandleDeviceError(dxgiFactory->CreateSwapChain(fD3dDevice.Get(), &scd, fD3dSwapChain.GetAddressOf())
                //    , "Could not create swap chain");
         

                D3D11Error::HandleDeviceError(

                    D3D11CreateDeviceAndSwapChain(
                        nullptr,
                        D3D_DRIVER_TYPE_HARDWARE,
                        nullptr,
                        createFlags,
                        requestedLevels,
                        sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
                        D3D11_SDK_VERSION,
                        &scd,
                        fD3dSwapChain.GetAddressOf(),
                        fD3dDevice.GetAddressOf(),
                        &obtainedLevel,
                        fD3dContext.GetAddressOf())
                    , "Could not create device");
            
#ifdef _DEBUG
            std::string obj = "D3D11 deivce";
            fD3dDevice->SetPrivateData(WKPDID_D3DDebugObjectName, obj.size(), obj.c_str());
#endif
            
        }
    private:
        HWND fHWND = nullptr;
        ComPtr<IDXGISwapChain> fD3dSwapChain;
        ComPtr<ID3D11DeviceContext> fD3dContext;
        ComPtr<ID3D11Device> fD3dDevice;
    };

}
