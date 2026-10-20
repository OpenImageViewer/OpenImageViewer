#include <OIVShared/AtomicFile.h>
#include <LLUtils/Exception.h>
#include <LLUtils/FileHandle.h>
#include <atomic>
#include <optional>
#ifdef _WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

namespace OIV
{
    void WriteFileAtomically(const std::filesystem::path& path, std::span<const std::byte> bytes)
    {
        static std::atomic_uint64_t nextFile{};
#ifdef _WIN32
        const auto process = GetCurrentProcessId();
#else
        const auto process = getpid();
#endif
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());
        std::filesystem::path temporary;
        std::optional<LLUtils::FileHandle> output;
        // CreateNew reserves a sibling exclusively. A stale file from a reused process ID must never be
        // overwritten or removed as though this writer owned it.
        while (!output)
        {
            temporary = path;
            temporary += "." + std::to_string(process) + "." + std::to_string(++nextFile) + ".tmp";
            try
            {
                output.emplace(LLUtils::FileHandle::OpenAppend(temporary, LLUtils::NativeFile::Disposition::CreateNew));
            }
            catch (const std::system_error& error)
            {
                if (error.code() != std::errc::file_exists)
                    throw;
            }
        }
        struct Cleanup
        {
            const std::filesystem::path& path;
            ~Cleanup()
            {
                std::error_code error;
                std::filesystem::remove(path, error);
            }
        } cleanup{temporary};
        const bool written = output->Write(bytes);
        const bool closed  = output->Close();
        if (!written || !closed)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Could not write an atomic file replacement");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
            LL_EXCEPTION_SYSTEM_ERROR("Could not publish an atomic file replacement");
#else
        std::filesystem::rename(temporary, path);
#endif
    }
}  // namespace OIV
