#pragma once

#include <thread>
#include <string>
#include <map>
#include <mutex>
#include <LLUtils/Event.h>
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/Exception.h>

class FileWatcher
{
    struct FolderData;
    using FILE_NOTIFY_INFORMATION = FILE_NOTIFY_INFORMATION;
public:
    enum class FileChangedOp { None, Add, Remove, Modified , Rename};
    struct FileChangedEventArgs
    {
        FileChangedOp fileOp;
        std::wstring folder;
        std::wstring fileName;
        std::wstring fileName2;
    };

    using OnFileChangedEventArgsEvent = LLUtils::Event<void(FileChangedEventArgs)>;

    OnFileChangedEventArgsEvent FileChangedEvent;

    bool IsFolderRegistered(const std::wstring& folder) const
    {
        return fMapFolderID.find(folder) != fMapFolderID.end();
    }

    void AddFolder(const std::wstring& folder)
    {
        std::lock_guard<std::mutex> Lock(fDataMutex);

        if (std::filesystem::is_directory(folder) == false)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "not a directory");

        auto it = fMapFolderID.find(folder);
        if (it != fMapFolderID.end())
        {
            using namespace std::string_literals;
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "the folder "s + LLUtils::StringUtility::ToAString(folder) + " already exists"s);
            //already exists
        }
        else
        {
            auto uniqueID = fUniqueIDProvider.Acquire();
            fMapFolderID.emplace(folder, uniqueID);
            auto it = fMapIDData.emplace(uniqueID, FolderData{});

            FolderData& folderData = it.first->second;

            folderData.uniqueID = uniqueID;
            folderData.folderPath = folder;
            folderData.directoryHandle = CreateFile(folder.c_str()
                , GENERIC_READ | FILE_LIST_DIRECTORY
                , FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE
                , nullptr
                , OPEN_EXISTING
                , FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED
                , nullptr);

            if (folderData.directoryHandle == nullptr)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Can not create directory");

            if (ReadDirectoryChanges(folderData) == 0)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Can not read directory changes");

            //Associate directory handle with a completion port.
            fCompletionPortHandle = CreateIoCompletionPort(folderData.directoryHandle, fCompletionPortHandle, static_cast<ULONG_PTR>(folderData.uniqueID), 0);

            if (fCompletionPortHandle == nullptr)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Can not associate completion port with the directory.");


            if (fFileWatchThread.native_handle() == nullptr)
                fFileWatchThread = std::thread(std::bind(&FileWatcher::CompletionPortStatusEntryPoint, this));
        }
    }

    void RemoveAll()
    {
        std::lock_guard<std::mutex> Lock(fDataMutex);
        for (const auto& [folder, id] : fMapFolderID)
        {
            auto it = fMapIDData.find(id);
            if (it == fMapIDData.end())
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Incoherent data structures.");

            if (CloseHandle(it->second.directoryHandle) == FALSE)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not close handle.");
        }

        fMapFolderID.clear();
        fMapIDData.clear();
        fUniqueIDProvider.Reset();
    }

    void RemoveFolder(const std::wstring& folder)
    {
        std::lock_guard<std::mutex> Lock(fDataMutex);
        auto it = fMapFolderID.find(folder);
        if (it == fMapFolderID.end())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Folder not found.");

        
        auto itData = fMapIDData.find(it->second);
        if (itData == fMapIDData.end())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Incoherent data structures.");

        if (CloseHandle(itData->second.directoryHandle) == FALSE)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Could not close handle.");

        fUniqueIDProvider.Release(itData->second.uniqueID);

        fMapFolderID.erase(it);
        fMapIDData.erase(itData);

    }
   
    static VOID CALLBACK QueueShutdownBackgroundThread(ULONG_PTR dwParam) 
    { 
        reinterpret_cast<FileWatcher*>(dwParam)->fQueueShutdownBackgroundThread = true;
    }

    void QueueShutdown()
    {
        QueueUserAPC(QueueShutdownBackgroundThread, fFileWatchThread.native_handle(), (ULONG_PTR)this);
    }

    ~FileWatcher()
    {
        RemoveAll();
        if (fFileWatchThread.joinable())
        {
            QueueShutdown();
            // wait for background thread to close.
            fFileWatchThread.join();
        }
    }

    void CompletionPortStatusEntryPoint()
    {
        while (fQueueShutdownBackgroundThread == false)
        {
            ULONG numEntiresReceived;
            constexpr ULONG maxEntires = 32;
            OVERLAPPED_ENTRY overlappedEntires[maxEntires];
            BOOL result = GetQueuedCompletionStatusEx(
                fCompletionPortHandle
                , overlappedEntires
                , maxEntires
                , &numEntiresReceived
                , INFINITE
                , TRUE);

            if (numEntiresReceived > maxEntires)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "some entries are unhandled, please complete the implementation");


            if (result == TRUE)
            {
                std::map<UniqueIDProvider::underlying_type, std::vector<FileChangedEventArgs>> eventsToRaise;
                {
                    std::lock_guard<std::mutex> lock(fDataMutex);
                    for (size_t i = 0; i < numEntiresReceived; i++)
                    {

                        const OVERLAPPED_ENTRY & overlappedEntry= overlappedEntires[i];
                        UniqueIDProvider::underlying_type folderID = static_cast<UniqueIDProvider::underlying_type>(overlappedEntry.lpCompletionKey);
                        auto it = fMapIDData.find(folderID);

                        if (it != fMapIDData.end())
                        {
                            FolderData& folderData = it->second;
                            bool done = false;
                            uint32_t currentOffset = 0;
                            do
                            {
                                FileChangedOp fileOp = FileChangedOp::None;
                                FILE_NOTIFY_INFORMATION* currentPacket = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<uint8_t*>(folderData.info) + currentOffset);

                                std::wstring fileName(currentPacket->FileName, currentPacket->FileNameLength / sizeof(wchar_t));
                                std::wstring newName;
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
                                        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "expected another packet");
                                    }
                                    else
                                    {
                                        currentPacket = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<uint8_t*>(folderData.info) + currentOffset);
                                        if (currentPacket->Action != FILE_ACTION_RENAMED_NEW_NAME)
                                        {
                                            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "expected a NEW_NAME packet packet");
                                        }
                                        newName = std::wstring(currentPacket->FileName, currentPacket->FileNameLength / sizeof(wchar_t));
                                    }

                                    break;
                                }

                                auto it = eventsToRaise.find(folderID);
                                if (it == eventsToRaise.end())
                                    it = eventsToRaise.emplace(folderID, std::vector< FileChangedEventArgs>()).first;

                                it->second.push_back(FileChangedEventArgs{ fileOp,folderData.folderPath,fileName,newName });
                                

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

                {
                    
                    //Lock mutex and restart Directory monitoring if directory is still registered.
                    std::lock_guard<std::mutex> lock(fDataMutex);

                    for (auto& [folderID, events] : eventsToRaise)
                    {
                        auto it = fMapIDData.find(folderID);
                        if (it != fMapIDData.end())
                        {
                            if (ReadDirectoryChanges(it->second) == 0)
                                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Can not read directory changes");
                        }
                    }
                }
            }
            else if (fQueueShutdownBackgroundThread == false)
            {
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "can not get completion status");
            }
        }
    }
    private:
        DWORD ReadDirectoryChanges(FolderData& folderData)
        {
            return
                ReadDirectoryChangesExW(folderData.directoryHandle
                    , &folderData.info
                    , BufferSize
                    , FALSE
                    , FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE
                    , nullptr
                    , &folderData.overlapped
                    , nullptr
                    , ReadDirectoryNotifyInformation);
        }
private:
    static constexpr uint32_t BufferSize = 8192;
    using UniqueIDProvider = LLUtils::UniqueIdProvider<uint16_t>;
    

    struct FolderData
    {
        alignas(DWORD) std::byte info[BufferSize]{};
        UniqueIDProvider::underlying_type uniqueID;
        OVERLAPPED overlapped{};
        HANDLE directoryHandle = nullptr;
        std::wstring folderPath;
    };

private:
    using MapFolderID = std::map < std::wstring, UniqueIDProvider::underlying_type>;
    using MapIDData = std::map < UniqueIDProvider::underlying_type, FolderData>;
    MapFolderID fMapFolderID;
    MapIDData fMapIDData;
    HANDLE fCompletionPortHandle = nullptr;
    std::mutex fDataMutex;
    std::thread fFileWatchThread;
    UniqueIDProvider fUniqueIDProvider{ 1 };
    bool fQueueShutdownBackgroundThread = false;
};

