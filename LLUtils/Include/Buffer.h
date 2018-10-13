#pragma once
#include <memory>
#include <stdexcept>
namespace LLUtils
{
    class STDAlloc
    {
    public:
        static std::byte* Allocate(size_t size)
        {
            return new std::byte[size];
        }

        static void Deallocate(std::byte* buffer)
        {
            delete[] buffer;
        }
    };


    class AlignedAlloc
    {
    public:
        static constexpr int Alignment = 16;

        static std::byte * Allocate(size_t size)
        {
            return reinterpret_cast<std::byte*>(_aligned_malloc(size, Alignment));
        }

        static void Deallocate(std::byte* buffer)
        {
            _aligned_free(buffer);
        }
    };


    template <typename Alloc>
    class BufferBase
    {
    public:
        using Allocator = Alloc;
        BufferBase(const BufferBase&) = delete;
        BufferBase& operator=(const BufferBase&) = delete;
        BufferBase() = default;
        BufferBase(size_t size)
        {
            Allocate(size);
        }


        void operator=(BufferBase&& rhs)
        {
            Swap(std::move(rhs));
        }

        BufferBase(BufferBase&& rhs)
        {
            Swap(std::move(rhs));
        }


        bool operator==(nullptr_t null) const
        {
            return fData == null;
        }

        BufferBase Clone() const
        {
            BufferBase cloned(fSize);
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

        void Write(const std::byte* BufferBase, size_t offset, size_t size)
        {
            if (offset + size <= fSize)
                memcpy(fData + offset, BufferBase, size);
            else
                throw std::runtime_error("Memory write overflow");
        }

        ~BufferBase()
        {
            Free();
        }

        size_t Size() const
        {
            return fSize;
        }

        // buffer must have been allocated with the corresponding Allocator.
        void TransferOwnership(size_t size, std::byte*& data)
        {
            fSize = size;
            fData = data;
            data = nullptr;
        }

        // buffer must be freed  with the corresponding Allocator.
        void RemoveOwnership(size_t& size , std::byte*& data)
        {
            size = fSize;
            fSize = 0;
            data = fData;
            fData = nullptr;
        }

    private:
        // private methods
        void Swap(BufferBase&& rhs)
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
            fData = Allocator::Allocate(size);
            fSize = size;
        }



        void FreeImpl()
        {
            if (fData != nullptr)
            {
                Allocator::Deallocate(fData);
                fData = nullptr;
                fSize = 0;
            }
        }

        // private member fields
        std::byte* fData = nullptr;
        size_t fSize = 0;
    };

    using Buffer = BufferBase<AlignedAlloc>;
}