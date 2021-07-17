#pragma once
#include <windows.h>
#include <d3dcommon.h>
#include <d3d11_2.h>
#include <dxgi1_4.h>
#include "D3D11Common.h"
#include "D3D11Error.h"
#include <cstdlib>

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
            D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0};
			
          

            UINT createFlags = 0;
            createFlags |= D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef _DEBUG
            createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif


            HRESULT res = D3D11CreateDevice(
                nullptr
                , D3D_DRIVER_TYPE_HARDWARE
                , nullptr
                , createFlags
                , requestedLevels
                , sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL)
                , D3D11_SDK_VERSION
                , fD3dDevice.GetAddressOf()
                , nullptr
                , fD3dContext.GetAddressOf());

            if (FAILED(res))
            {
                res = D3D11CreateDevice(
                    nullptr
                    , D3D_DRIVER_TYPE_WARP
                    , nullptr
                    , createFlags
                    , requestedLevels
                    , sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL)
                    , D3D11_SDK_VERSION
                    , fD3dDevice.GetAddressOf()
                    , nullptr
                    , fD3dContext.GetAddressOf());
            }

            if (FAILED(res))
                D3D11Error::HandleDeviceError(res, "Could not create D3D11 device");
                
            

				ComPtr<IDXGIDevice> dxgiDevice;
				ComPtr<IDXGIAdapter> dxgiAdapter;
				ComPtr<IDXGIFactory2>  dxgiFactory;

				if (SUCCEEDED(fD3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.GetAddressOf()))))
					if (SUCCEEDED(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(dxgiAdapter.GetAddressOf()))))
						if (SUCCEEDED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(dxgiFactory.GetAddressOf()))))
						{

						}

				if (dxgiFactory == nullptr)
					LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "This software requires windows 7 with platform update or higher");


                DXGI_SWAP_CHAIN_DESC1 scd{};

                ComPtr<IDXGIFactory4>  dxgiFactory4;
                if (SUCCEEDED(dxgiFactory->QueryInterface(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(dxgiFactory4.GetAddressOf()))))
                {
                    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                    scd.Scaling = DXGI_SCALING_NONE;
                }
                else
                {
                    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
                    scd.Scaling = DXGI_SCALING_STRETCH;
                }

                

                scd.BufferCount = 2;
                scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                
                scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
                scd.Width = 1280;
                scd.Height = 800;
                scd.Stereo = false;
                scd.SampleDesc.Count = 1;
                scd.SampleDesc.Quality = 0;
                scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                scd.Flags = static_cast<UINT>(0);


				dxgiFactory->CreateSwapChainForHwnd(fD3dDevice.Get(), fHWND, &scd, nullptr, nullptr, fD3dSwapChain.GetAddressOf());
				dxgiFactory->MakeWindowAssociation(fHWND, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);


        OIV_D3D_SET_OBJECT_NAME(fD3dDevice, "D3D11 device");
        OIV_D3D_SET_OBJECT_NAME(fD3dSwapChain, "D3D11 swap chain");
        OIV_D3D_SET_OBJECT_NAME(fD3dContext, "D3D11 context");
        }
    private:
        HWND fHWND = nullptr;
        ComPtr<IDXGISwapChain1> fD3dSwapChain;
        ComPtr<ID3D11DeviceContext> fD3dContext;
        ComPtr<ID3D11Device> fD3dDevice;
    };

}
