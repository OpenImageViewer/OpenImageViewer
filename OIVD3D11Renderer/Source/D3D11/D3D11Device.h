#pragma once
#include <windows.h>
#include <d3dcommon.h>
#include <d3d11_2.h>
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
            D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0 };
			
            DXGI_SWAP_CHAIN_DESC1 scd{};

            scd.BufferCount = 2;
            scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			scd.Scaling = DXGI_SCALING_NONE;
			scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			scd.Width = 1280;
			scd.Height = 800;
			scd.Stereo = false;
			scd.SampleDesc.Count = 1;
			scd.SampleDesc.Quality = 0;
            scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			scd.Flags = static_cast<UINT>(0);
			scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			

            UINT createFlags = 0;
            createFlags |= D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef _DEBUG
            createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

                D3D11CreateDevice(
                      nullptr
                    , D3D_DRIVER_TYPE_HARDWARE
                    , nullptr
                    , createFlags
                    , requestedLevels
                    , sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL)
                    , D3D11_SDK_VERSION
                    , fD3dDevice.GetAddressOf()
                    , nullptr
                    , fD3dContext.GetAddressOf()
                );

				ComPtr<IDXGIDevice> dxgiDevice;
				ComPtr<IDXGIAdapter> dxgiAdapter;
				ComPtr<IDXGIFactory2>  dxgiFactory;

				if (SUCCEEDED(fD3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.GetAddressOf()))))
					if (SUCCEEDED(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), reinterpret_cast<void**>(dxgiAdapter.GetAddressOf()))))
						if (SUCCEEDED(dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(dxgiFactory.GetAddressOf()))))
						{

						}

				if (dxgiFactory == nullptr)
					LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "This software requires windows 8.1 or higher");


				dxgiFactory->CreateSwapChainForHwnd(fD3dDevice.Get(), fHWND, &scd, nullptr, nullptr, fD3dSwapChain.GetAddressOf());
				dxgiFactory->MakeWindowAssociation(fHWND, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);

			
			/*
                D3D11Error::HandleDeviceError(

                    D3D11CreateDeviceAndSwapChain (
                          nullptr
                        , D3D_DRIVER_TYPE_HARDWARE
                        , nullptr
                        , createFlags
                        , requestedLevels
                        , sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL)
                        , D3D11_SDK_VERSION
                        , &scd
                        , fD3dSwapChain.GetAddressOf()
                        , fD3dDevice.GetAddressOf()
                        , &obtainedLevel
                        , fD3dContext.GetAddressOf())
                    , "Could not create device");
        

		*/

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
