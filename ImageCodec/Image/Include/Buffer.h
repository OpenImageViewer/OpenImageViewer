#pragma once
#include <cmath>
#include <cstdint>
#include <memory>

class Buffer
{
public:
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;



    Buffer() = default;


    void operator=(Buffer&& rhs)
    {
        Swap(std::move(rhs));
    }

    Buffer(Buffer&& rhs)
    {
        Swap(std::move(rhs));
    }


    void Swap(Buffer&& rhs)
    {
        std::swap(fData, rhs.fData);
        _aligned_free(rhs.fData);
        rhs.fData = nullptr;
    }

    const uint8_t* GetConstBuffer() const
    {
        return fData;
    }

    uint8_t* GetBuffer() const
    {
        return fData;
    }

    void Free()
    {
        if (fData != nullptr)
        {
            _aligned_free(fData);
            fData = nullptr;
        }
    }

    void AllocateAndWrite(const uint8_t* buffer, size_t size)
    {
        Allocate(size);
        Write(buffer, size);
    }

    void Allocate(size_t size)
    {
        AllocateImp(size);
    }

    void Write(const uint8_t* buffer, size_t size)
    {
        memcpy(fData, buffer, size);
    }

    ~Buffer()
    {
        Free();
    }
private:
    void AllocateImp(size_t size)
    {
        Free();
        fData = reinterpret_cast<uint8_t*>(_aligned_malloc(size, 16));

    }

    uint8_t* fData = nullptr;
};
