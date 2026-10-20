#pragma once
#include <OIVAppCore/FileAssociationPolicy.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace OIV
{
    // The worker retains the cross-process prompt lock through the response and native operation.
    // Destruction cancels the wait and joins without depending on UI dispatch.
    class FileAssociationSession final
    {
      public:

        // Backend notifications run synchronously on this worker and must not be retained. Calls are sequential.
        // Prompt responses are handled only after Check returns and releases its native resources.
        using Backend = std::function<void(FileAssociationAction, std::stop_token, const FileAssociationNotify&)>;
        FileAssociationSession(std::filesystem::path configDirectory, LLUtils::native_string_type executable,
                               Backend backend, FileAssociationNotify notify,
                               FileAssociationAction action = FileAssociationAction::Check);
        ~FileAssociationSession();
        void Respond(FileAssociationChoice choice);
        bool IsRunning() const { return fRunning; }

      private:

        std::mutex fMutex;
        std::condition_variable_any fResponseReady;
        std::optional<FileAssociationChoice> fResponse;
        std::atomic_bool fRunning{true};
        std::jthread fWorker;
    };
}  // namespace OIV
