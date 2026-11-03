#pragma once

#include <LLUtils/Thread.h>
#include <Windows.h>

#include <thread>
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <limits>
#include <OIVAppCore/IFileWatcher.h>
#include <LLUtils/Event.h>
#include <LLUtils/Exception.h>

namespace OIV
{
    namespace Win32
    {

        class FileWatcherWin32 : public OIV::IFileWatcher
        {
          private:

            struct FolderData;
            using FILE_NOTIFY_INFORMATION = FILE_NOTIFY_INFORMATION;

          public:

            using FolderID                    = OIV::IFileWatcher::FolderID;
            using FileChangedOp               = OIV::IFileWatcher::FileChangedOp;
            using FileChangedEventArgs        = OIV::IFileWatcher::FileChangedEventArgs;
            using OnFileChangedEventArgsEvent = OIV::IFileWatcher::OnFileChangedEventArgsEvent;

            OnFileChangedEventArgsEvent FileChangedEvent;

            bool IsFolderRegistered(const LLUtils::native_string_type& folder) const override
            {
                std::lock_guard<std::mutex> lock(fDataMutex);
                return fMapFolderID.find(folder) != fMapFolderID.end();
            }

            FolderID AddFolder(const LLUtils::native_string_type& folder) override
            {
                std::lock_guard<std::mutex> lock(fDataMutex);
                if (fShutdownRequested.load())
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "File watcher has stopped");
                if (!std::filesystem::is_directory(folder))
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "not a directory");
                if (fMapFolderID.contains(folder))
                {
                    using namespace std::string_literals;
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem,
                                 "the folder "s + LLUtils::StringUtility::ToAString(folder) + " already exists"s);
                }
                // An old notification can remain in the completion port, a watcher callback, or the
                // client queue after its folder is removed. Reusing its ID could make it appear to
                // belong to a new watch, so IDs remain unique for the lifetime of this watcher.
                // Reuse could be supported by coordinating cancellation and callback completion with
                // draining queued and in-progress client events, or by using generation identities.
                // These alternatives add coordination and have not been fully explored. A better
                // solution may exist. The completion port key also limits IDs to ULONG_PTR width.
                if (fNextFolderID >= std::numeric_limits<ULONG_PTR>::max())
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "File watcher IDs exhausted");
                const FolderID id = fNextFolderID++;
                auto it = fMapIDData.try_emplace(id).first;
                FolderData& data = it->second;
                try
                {
                    data.folderPath = folder;
                    fMapFolderID.emplace(folder, id);
                    data.completionEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                    if (data.completionEvent == nullptr)
                        LL_EXCEPTION_SYSTEM_ERROR("Can not create directory watch event");
                    data.directoryHandle = CreateFile(folder.c_str(), GENERIC_READ | FILE_LIST_DIRECTORY,
                                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                      nullptr, OPEN_EXISTING,
                                                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
                    if (data.directoryHandle == INVALID_HANDLE_VALUE)
                        LL_EXCEPTION_SYSTEM_ERROR("Can not open watched directory");
                    const HANDLE port = CreateIoCompletionPort(data.directoryHandle, fCompletionPortHandle,
                                                                static_cast<ULONG_PTR>(id), 0);
                    if (port == nullptr)
                        LL_EXCEPTION_SYSTEM_ERROR("Can not associate directory with completion port");
                    fCompletionPortHandle = port;
                    if (!ReadDirectoryChanges(data))
                        LL_EXCEPTION_SYSTEM_ERROR("Can not read directory changes");
                    if (!fFileWatchThread.joinable())
                        fFileWatchThread = LLUtils::StartThread(&FileWatcherWin32::CompletionPortStatusEntryPoint, this);
                }
                catch (...)
                {
                    if (data.directoryHandle != INVALID_HANDLE_VALUE)
                    {
                        CancelAndDrain(data);
                        CloseHandle(data.directoryHandle);
                    }
                    if (data.completionEvent != nullptr) CloseHandle(data.completionEvent);
                    fMapFolderID.erase(folder);
                    fMapIDData.erase(it);
                    throw;
                }
                return id;
            }

            void RemoveAll()
            {
                std::lock_guard<std::mutex> lock(fDataMutex);
                while (!fMapIDData.empty())
                    RemoveFolderLocked(fMapIDData.begin()->first);
            }

            void RemoveFolder(FolderID folderID) override
            {
                std::lock_guard<std::mutex> Lock(fDataMutex);
                RemoveFolderLocked(folderID);
            }

            void RemoveFolder(const LLUtils::native_string_type& folder) override
            {
                std::lock_guard<std::mutex> Lock(fDataMutex);
                auto it = fMapFolderID.find(folder);
                if (it == fMapFolderID.end())
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Folder not found.");

                RemoveFolderLocked(it->second);
            }

            OnFileChangedEventArgsEvent& GetFileChangedEvent() override { return FileChangedEvent; }

            static VOID CALLBACK WakeAlertableWait(ULONG_PTR) {}

            void StopNotifications() override
            {
                fShutdownRequested.store(true);
                if (fFileWatchThread.joinable())
                {
                    // The watcher thread blocks in GetQueuedCompletionStatusEx(..., INFINITE, TRUE). The TRUE argument
                    // makes the wait alertable, so this APC only wakes that wait; shutdown state is set above and the
                    // APC must not dereference this object.
                    QueueUserAPC(WakeAlertableWait, reinterpret_cast<HANDLE>(fFileWatchThread.native_handle()), 0);
                    // wait for background thread to close.
                    fFileWatchThread.join();
                }
            }

            void Shutdown()
            {
                StopNotifications();
                RemoveAll();
                if (fCompletionPortHandle != nullptr)
                {
                    CloseHandle(fCompletionPortHandle);
                    fCompletionPortHandle = nullptr;
                }
            }

            ~FileWatcherWin32() { Shutdown(); }

            void CompletionPortStatusEntryPoint()
            {
                bool shutdownRequested = fShutdownRequested.load();
                while (shutdownRequested == false)
                {
                    ULONG numEntiresReceived   = 0;
                    constexpr ULONG maxEntires = 32;
                    OVERLAPPED_ENTRY overlappedEntires[maxEntires];
                    BOOL result = GetQueuedCompletionStatusEx(fCompletionPortHandle, overlappedEntires, maxEntires,
                                                              &numEntiresReceived, INFINITE, TRUE);
                    shutdownRequested = fShutdownRequested.load();

                    if (result == TRUE && shutdownRequested == false)
                    {
                        if (numEntiresReceived > maxEntires)
                            LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented,
                                         "some entries are unhandled, please complete the implementation");

                        std::map<FolderID, std::vector<FileChangedEventArgs>> eventsToRaise;
                        {
                            std::lock_guard<std::mutex> lock(fDataMutex);
                            for (size_t i = 0; i < numEntiresReceived; i++)
                            {
                                const OVERLAPPED_ENTRY& overlappedEntry = overlappedEntires[i];
                                FolderID folderID = static_cast<FolderID>(overlappedEntry.lpCompletionKey);
                                auto it           = fMapIDData.find(folderID);

                                if (it != fMapIDData.end() && it->second.pending &&
                                    overlappedEntry.lpOverlapped == &it->second.overlapped)
                                {
                                    FolderData& folderData = it->second;
                                    folderData.pending = false;
                                    if (overlappedEntry.Internal != 0 || overlappedEntry.dwNumberOfBytesTransferred == 0)
                                    {
                                        eventsToRaise.try_emplace(folderID);
                                        continue;
                                    }
                                    bool done              = false;
                                    uint32_t currentOffset = 0;
                                    do
                                    {
                                        FileChangedOp fileOp = FileChangedOp::None;
                                        FILE_NOTIFY_INFORMATION* currentPacket =
                                            reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                                                reinterpret_cast<uint8_t*>(folderData.info) + currentOffset);

                                        LLUtils::native_string_type fileName(
                                            currentPacket->FileName, currentPacket->FileNameLength / sizeof(wchar_t));
                                        LLUtils::native_string_type newName;
                                        switch (currentPacket->Action)
                                        {
                                            case FILE_ACTION_ADDED:
                                                fileOp = FileChangedOp::Add;
                                                break;
                                            case FILE_ACTION_REMOVED:
                                                fileOp = FileChangedOp::Remove;
                                                break;
                                            case FILE_ACTION_MODIFIED:
                                                fileOp = FileChangedOp::Modified;
                                                break;
                                            case FILE_ACTION_RENAMED_OLD_NAME:
                                                fileOp = FileChangedOp::Rename;
                                                currentOffset += currentPacket->NextEntryOffset;
                                                if (currentPacket->NextEntryOffset == 0)
                                                {
                                                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState,
                                                                 "expected another packet");
                                                }
                                                else
                                                {
                                                    currentPacket = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                                                        reinterpret_cast<uint8_t*>(folderData.info) + currentOffset);
                                                    if (currentPacket->Action != FILE_ACTION_RENAMED_NEW_NAME)
                                                    {
                                                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState,
                                                                     "expected a NEW_NAME packet packet");
                                                    }
                                                    newName = LLUtils::native_string_type(
                                                        currentPacket->FileName,
                                                        currentPacket->FileNameLength / sizeof(wchar_t));
                                                }

                                                break;
                                        }

                                        auto it = eventsToRaise.find(folderID);
                                        if (it == eventsToRaise.end())
                                            it = eventsToRaise.emplace(folderID, std::vector<FileChangedEventArgs>())
                                                     .first;

                                        it->second.push_back(FileChangedEventArgs{
                                            folderID, fileOp, folderData.folderPath, fileName, newName});

                                        currentOffset += currentPacket->NextEntryOffset;
                                        if (currentPacket->NextEntryOffset == 0)
                                            done = true;
                                    } while (!done);
                                }
                            }
                        }

                        // Unlock mutex and Raise events
                        for (auto& [folderID, events] : eventsToRaise)
                        {
                            for (const auto& eventArgs : events)
                                FileChangedEvent.Raise(eventArgs);
                        }

                        std::set<FolderID> foldersToRemove;

                        std::vector<FileChangedEventArgs> folderRemovalEvents;
                        {
                            // Lock mutex and restart Directory monitoring if directory is still registered.
                            std::lock_guard<std::mutex> lock(fDataMutex);
                            for (auto& [folderID, events] : eventsToRaise)
                            {
                                auto it = fMapIDData.find(folderID);
                                if (it != fMapIDData.end())
                                {
                                    // TODO: handle directory removal using a better way, e.g. an event from the system.
                                    if (std::filesystem::exists(it->second.folderPath))
                                    {
                                        if (ReadDirectoryChanges(it->second) == 0)
                                            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState,
                                                         "Can not read directory changes");
                                    }
                                    else
                                    {
                                        foldersToRemove.insert(folderID);
                                    }
                                }
                            }

                            for (FolderID folderID : foldersToRemove)
                            {
                                auto it = fMapIDData.find(folderID);
                                folderRemovalEvents.push_back(FileChangedEventArgs{
                                    folderID, FileChangedOp::WatchedFolderRemoved, it->second.folderPath,
                                    LLUtils::native_string_type(), LLUtils::native_string_type()});
                                RemoveFolderLocked(folderID);
                            }
                        }

                        // Unlock mutex and Raise events
                        for (const auto& eventArgs : folderRemovalEvents)
                            FileChangedEvent.Raise(eventArgs);
                    }
                    else if (shutdownRequested == false)
                    {
                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "can not get completion status");
                    }
                }
            }

          private:

            void CancelAndDrain(FolderData& data)
            {
                if (!data.pending) return;
                if (!CancelIoEx(data.directoryHandle, &data.overlapped) && GetLastError() != ERROR_NOT_FOUND)
                    LL_EXCEPTION_SYSTEM_ERROR("Could not cancel directory watch");
                DWORD transferred = 0;
                const BOOL completed = GetOverlappedResult(data.directoryHandle, &data.overlapped, &transferred, TRUE);
                if (!completed && !HasOverlappedIoCompleted(&data.overlapped))
                    LL_EXCEPTION_SYSTEM_ERROR("Could not wait for directory watch cancellation");
                data.pending = false;
            }

            void RemoveFolderLocked(FolderID folderID)
            {
                auto itData = fMapIDData.find(folderID);
                if (itData == fMapIDData.end())
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Incoherent data structures.");
                CancelAndDrain(itData->second);
                if (!CloseHandle(itData->second.directoryHandle))
                    LL_EXCEPTION_SYSTEM_ERROR("Could not close watched directory");
                CloseHandle(itData->second.completionEvent);
                fMapFolderID.erase(itData->second.folderPath);
                fMapIDData.erase(itData);
            }

            DWORD ReadDirectoryChanges(FolderData& data)
            {
                if (data.pending)
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Directory watch is already pending");
                data.overlapped = {};
                data.overlapped.hEvent = data.completionEvent;
                if (!ResetEvent(data.completionEvent))
                    LL_EXCEPTION_SYSTEM_ERROR("Could not reset directory watch event");
                const DWORD result = ReadDirectoryChangesW(data.directoryHandle, &data.info, BufferSize, FALSE,
                                                            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                                            nullptr, &data.overlapped, nullptr);
                data.pending = result != 0;
                return result;
            }

          private:

            static constexpr uint32_t BufferSize = 8192;

            struct FolderData
            {
                alignas(DWORD) std::byte info[BufferSize]{};
                OVERLAPPED overlapped{};
                HANDLE directoryHandle = INVALID_HANDLE_VALUE;
                HANDLE completionEvent = nullptr;
                bool pending = false;
                LLUtils::native_string_type folderPath;
            };

          private:

            using MapFolderID = std::map<LLUtils::native_string_type, FolderID>;
            using MapIDData   = std::map<FolderID, FolderData>;
            MapFolderID fMapFolderID;
            MapIDData fMapIDData;
            HANDLE fCompletionPortHandle = nullptr;
            mutable std::mutex fDataMutex;
            std::thread fFileWatchThread;
            FolderID fNextFolderID = 1;
            std::atomic_bool fShutdownRequested = false;
        };

    }  // namespace Win32
}  // namespace OIV
