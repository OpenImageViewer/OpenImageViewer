#pragma once
#include <memory>
#include <wrl/client.h>

namespace OIV
{
    enum class ShaderStage
    {
          None
        , VertexShader
        , GeometryShader
        , FragmentShader
    };

    class D3D11Device;
    typedef std::shared_ptr<D3D11Device>  D3D11DeviceSharedPtr;
    
    
    template < typename T >
    using ComPtr = Microsoft::WRL::ComPtr<T>;

#if defined(_DEBUG)
    #if !defined(D3D_SET_OBJECT_NAME_A) && defined(__MINGW32__)
        #define OIV_D3D_SET_OBJECT_NAME(OBJ,NAME) (OBJ)->SetPrivateData(WKPDID_D3DDebugObjectName, strlen(NAME), NAME)
    #else
        #define OIV_D3D_SET_OBJECT_NAME(OBJ,NAME) D3D_SET_OBJECT_NAME_A(OBJ, NAME)
    #endif
#else 
    #define OIV_D3D_SET_OBJECT_NAME(OBJ,NAME)
#endif
}
