#pragma once
#include <memory>
namespace OIV
{
    class D3D11Device;
    typedef std::shared_ptr<D3D11Device>  D3D11DeviceSharedPtr;
    
    struct Blob;
    typedef std::shared_ptr<Blob>  BlobSharedPtr;

    #define SAFE_RELEASE(x) {if (x) {x->Release(); x = nullptr; } }
}