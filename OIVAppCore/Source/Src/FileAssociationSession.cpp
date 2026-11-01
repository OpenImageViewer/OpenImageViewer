#include <OIVAppCore/FileAssociationSession.h>
#include <OIVAppCore/FileAssociationPreferences.h>
#include <OIVAppCore/FileLock.h>

namespace OIV
{
    FileAssociationSession::FileAssociationSession(std::filesystem::path configDirectory,
                                                   LLUtils::native_string_type executable, Backend backend,
                                                   FileAssociationNotify notify, FileAssociationAction action)
        : fWorker(
              [this, config = std::move(configDirectory), executable = std::move(executable),
               backend = std::move(backend), notify = std::move(notify), action](std::stop_token stop)
              {
                  struct Completion
                  {
                      std::atomic_bool& running;
                      ~Completion() { running = false; }
                  } completion{fRunning};
                  auto failureKind = action == FileAssociationAction::Check ? FileAssociationNotice::Kind::CheckFailed
                                                                            : FileAssociationNotice::Kind::ActionFailed;
                  // Protect reporting as well as native work. Notification allocation can fail during teardown.
                  try
                  {
                      std::filesystem::create_directories(config);
                      FileLock promptLock(config / "associations.lock");
                      if (!promptLock.Lock(true, false))
                      {
                          if (action != FileAssociationAction::Check)
                              notify({FileAssociationNotice::Kind::ActionFailed,
                                      LLUTILS_TEXT("Another viewer is managing file associations.")});
                          return;
                      }
                      const auto location = FileAssociationPolicy::ExecutableLocation(executable);
                      if (stop.stop_requested() ||
                          (action == FileAssociationAction::Check && IsAssociationPromptSuppressed(config, location)))
                          return;
                      bool promptRequested = false;
                      backend(action, stop,
                              [&](FileAssociationNotice notice)
                              {
                                  if (notice.kind == FileAssociationNotice::Kind::Prompt)
                                  {
                                      promptRequested = true;
                                      failureKind     = FileAssociationNotice::Kind::ActionFailed;
                                  }
                                  notify(std::move(notice));
                              });
                      // Finish native checking before waiting for the UI. Keep the prompt lock held so
                      // releasing backend resources cannot let another process display a competing prompt.
                      if (promptRequested)
                      {
                          std::unique_lock lock(fMutex);
                          const bool answered = fResponseReady.wait(lock, stop,
                                                                    [this] { return fResponse.has_value(); });
                          const auto choice   = fResponse.value_or(FileAssociationChoice::AskNextTime);
                          lock.unlock();
                          // Commit a permanent response even if shutdown immediately cancels native work.
                          if (answered)
                              SaveAssociationPromptChoice(config, location, choice);
                          if (answered && choice == FileAssociationChoice::Associate && !stop.stop_requested())
                              backend(FileAssociationAction::Add, stop, notify);
                      }
                  }
                  catch (...)
                  {
                      try
                      {
                          if (!stop.stop_requested())
                              notify({failureKind, LLUTILS_TEXT("The file association operation failed.")});
                      }
                      catch (...)
                      {
                      }
                  }
              })
    {
    }
    FileAssociationSession::~FileAssociationSession() = default;
    void FileAssociationSession::Respond(FileAssociationChoice choice)
    {
        {
            const std::scoped_lock lock(fMutex);
            fResponse = choice;
        }
        fResponseReady.notify_one();
    }
}  // namespace OIV
