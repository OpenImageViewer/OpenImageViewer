#pragma once
#include <memory>
#include <stdexcept>
namespace LLUtils
{
    class Buffer
    {
    public:
        static constexpr int Alignment = 16;

        static std::byte * GlobalAllocate(size_t size)
        {
            return reinterpret_cast<std::byte*>(_aligned_malloc(size, Alignment));
        }

        static void GlobalDealocate(std::byte* buffer)
        {
            _aligned_free(buffer);
        }


        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        Buffer() = default;
        Buffer(size_t size)
        {
            Allocate(size);
        }


        void operator=(Buffer&& rhs)
        {
            Swap(std::move(rhs));
        }

        Buffer(Buffer&& rhs)
        {
            Swap(std::move(rhs));
        }


        bool operator==(nullptr_t null) const
        {
            return fData == null;
        }

        Buffer Clone() const
        {
            Buffer cloned(fSize);
            cloned.Write(fData, 0, fSize);
            return cloned;
        }


        const std::byte* GetBuffer() const
        {
            return fData;
        }

        std::byte* GetBuffer()
        {
            return fData;
        }

        void Free()
        {
            FreeImpl();
        }

        void Allocate(size_t size)
        {
            AllocateImp(size);
        }

        void Read(std::byte* dest, size_t offset, size_t size) const
        {
            if (offset + size <= fSize)
                memcpy(dest, fData + offset, size);
            else
                throw std::runtime_error("Memory read overflow");
        }

        void Write(const std::byte* buffer, size_t offset, size_t size)
        {
            if (offset + size <= fSize)
                memcpy(fData + offset, buffer, size);
            else
                throw std::runtime_error("Memory write overflow");
        }

        ~Buffer()
        {
            Free();
        }

        size_t Size() const
        {
            return fSize;
        }

        // buffer must have been allocated with Buffer::GlobalAllocate

        void TransferOwnership(size_t size, std::byte*& data)
        {
            fSize = size;
            fData = data;
            data = nullptr;
        }

        // buffer must be freed  with Buffer::GlobalDealocate
        void RemoveOwnership(size_t& size , std::byte*& data)
        {
            size = fSize;
            fSize = 0;
            data = fData;
            fData = nullptr;
        }

    private:
        // private methods
        void Swap(Buffer&& rhs)
        {
            std::swap(fSize, rhs.fSize);
            rhs.fSize = 0;
            std::swap(fData, rhs.fData);
            _aligned_free(rhs.fData);
            rhs.fData = nullptr;
        }

        void AllocateImp(size_t size)
        {
            Free();
            fData = GlobalAllocate(size);
            fSize = size;
        }



        void FreeImpl()
        {
            if (fData != nullptr)
            {
                GlobalDealocate(fData);
                fData = nullptr;
                fSize = 0;
            }
        }

        // private member fields
        std::byte* fData = nullptr;
        size_t fSize = 0;
    };
}