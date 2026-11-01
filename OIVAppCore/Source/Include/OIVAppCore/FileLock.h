#pragma once
#include <cstdint>
#include <filesystem>

namespace OIV
{
    // Locks the first byte of a stable coordination file. Closing releases every held lock.
    class FileLock final
    {
      public:

        explicit FileLock(const std::filesystem::path& path);
        ~FileLock();
        FileLock(const FileLock&)            = delete;
        FileLock& operator=(const FileLock&) = delete;
        // A nonblocking conflict returns false. Other native failures throw.
        bool Lock(bool exclusive, bool wait);
        void Unlock();

      private:

        // NativeFile encodes a Windows handle directly and a POSIX descriptor plus one.
        std::uintptr_t fDescriptor;
    };
}  // namespace OIV