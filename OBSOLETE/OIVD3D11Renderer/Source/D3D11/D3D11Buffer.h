#pragma once
#include "D3D11Error.h"
#include "D3D11Common.h"
#include "Exception.h"

namespace OIV
{
    
    class D3D11Buffer
    {
    public:
        D3D11Buffer(D3D11DeviceSharedPtr device, const D3D11_BUFFER_DESC& bufferDesc, const D3D11_SUBRESOURCE_DATA* initialData)
        {
            fDevice = device;
            fBufferDesc = bufferDesc;
            Create(initialData);
        }

        uint32_t GetSize() const
        {
            return fBufferDesc.ByteWidth;
        }

        void Use(ShaderStage stage,int32_t slot)
        {
            switch (stage)
            {
            case ShaderStage::VertexShader:
                GetDevice()->GetContext()->VSSetConstantBuffers(static_cast<UINT>(slot), 1, fBuffer.GetAddressOf());
                break;
            case ShaderStage::FragmentShader:
                GetDevice()->GetContext()->PSSetConstantBuffers(static_cast<UINT>(slot), 1, fBuffer.GetAddressOf());
                break;
            default:
                LL_EXCEPTION_UNEXPECTED_VALUE;
            }
            
        }

        void Write(uint32_t size, const uint8_t* buffer, uint32_t offset)
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            D3D11Error::HandleDeviceError(fDevice->GetContext()->Map(fBuffer.Get(), static_cast<UINT>(0), D3D11_MAP_WRITE_DISCARD, static_cast<UINT>(0), &mapped), "Can not map constant buffer");
            memcpy(static_cast<uint8_t*>(mapped.pData) + offset, buffer, size);
            fDevice->GetContext()->Unmap(fBuffer.Get(), static_cast<UINT>(0));
        }

    protected:
        D3D11DeviceSharedPtr GetDevice() const
        {
            return fDevice;
        }

        
    private:

        void Create(const D3D11_SUBRESOURCE_DATA* initialData)
        {
            if (fBufferDesc.BindFlags == D3D11_BIND_CONSTANT_BUFFER)
                fBufferDesc.ByteWidth =  LLUtils::Utility::Align(fBufferDesc.ByteWidth,static_cast<UINT>(16));


                D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateBuffer(&fBufferDesc, initialData,
                    fBuffer.ReleaseAndGetAddressOf())
                    , "Can not create constant buffer");

            OIV_D3D_SET_OBJECT_NAME(fBuffer, "Constant buffer");
        }


    private:
        ComPtr<ID3D11Buffer> fBuffer;
        D3D11DeviceSharedPtr fDevice;
        D3D11_BUFFER_DESC fBufferDesc;
    };

    typedef std::unique_ptr<D3D11Buffer> D3D11BufferUniquePtr;

    template <class T>
    class D3D11BufferBound : public D3D11Buffer
    {
    private:
        T fBufferData;

    public:
        D3D11BufferBound(D3D11DeviceSharedPtr device, const D3D11_BUFFER_DESC& bufferDesc, const D3D11_SUBRESOURCE_DATA* initialData)
            : D3D11Buffer(device,bufferDesc, initialData)
        {


        }

        void Update()
        {
            Write(sizeof(T), reinterpret_cast<const uint8_t*>(&fBufferData), 0);
        }

        T& GetBuffer()
        {
            return fBufferData;
        }

    };
    template<class T>
    using D3D11BufferBoundUniquePtr = std::unique_ptr < D3D11BufferBound<T>>;
    
}
