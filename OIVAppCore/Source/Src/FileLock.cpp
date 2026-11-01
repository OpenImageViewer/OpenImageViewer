#include <OIVAppCore/FileLock.h>
#include <LLUtils/NativeFile.h>
#include <LLUtils/Exception.h>

namespace OIV
{
    FileLock::FileLock(const std::filesystem::path& path)
        : fDescriptor(LLUtils::NativeFile::Open(path, LLUtils::NativeFile::Access::ReadWrite,
                                                LLUtils::NativeFile::Disposition::OpenOrCreate)
                          .Release())
    {
    }
    FileLock::~FileLock()
    {
#ifdef _WIN32
        CloseHandle(reinterpret_cast<HANDLE>(fDescriptor));
#else
        close(static_cast<int>(fDescriptor) - 1);
#endif
    }
    bool FileLock::Lock(bool exclusive, bool wait)
    {
#ifdef _WIN32
        OVERLAPPED overlap{};
        const DWORD flags = (exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0) | (wait ? 0 : LOCKFILE_FAIL_IMMEDIATELY);
        if (LockFileEx(reinterpret_cast<HANDLE>(fDescriptor), flags, 0, 1, 0, &overlap))
            return true;
        if (!wait && GetLastError() == ERROR_LOCK_VIOLATION)
            return false;
#else
        if (flock(static_cast<int>(fDescriptor) - 1, (exclusive ? LOCK_EX : LOCK_SH) | (wait ? 0 : LOCK_NB)) == 0)
            return true;
        if (!wait && (errno == EWOULDBLOCK || errno == EAGAIN))
            return false;
#endif
        LL_EXCEPTION_SYSTEM_ERROR("Could not acquire the OIViewer file lock");
    }
    void FileLock::Unlock()
    {
#ifdef _WIN32
        OVERLAPPED overlap{};
        if (!UnlockFileEx(reinterpret_cast<HANDLE>(fDescriptor), 0, 1, 0, &overlap))
            LL_EXCEPTION_SYSTEM_ERROR("Could not release the OIViewer file lock");
#else
        if (flock(static_cast<int>(fDescriptor) - 1, LOCK_UN) != 0)
            LL_EXCEPTION_SYSTEM_ERROR("Could not release the OIViewer file lock");
#endif
    }

}  // namespace OIV