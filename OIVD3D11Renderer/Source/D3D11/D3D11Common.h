#pragma once
#include <memory>
#include <wrl/client.h>

namespace OIV
{
    enum ShaderStage
    {
          SS_None
        , SS_VertexShader
        , SS_GeometryShader
        , SS_FragmentShader
    };

    class D3D11Device;
    typedef std::shared_ptr<D3D11Device>  D3D11DeviceSharedPtr;
    
    struct Blob;
    typedef std::shared_ptr<Blob>  BlobSharedPtr;
    
    template < typename T >
    using ComPtr = Microsoft::WRL::ComPtr<T>;
}
