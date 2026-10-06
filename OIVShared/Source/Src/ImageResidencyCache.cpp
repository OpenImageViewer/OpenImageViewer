#include <LLUtils/Logging/Logger.h>
#include <OIVShared/ImageResidencyCache.h>

namespace
{
    LLUtils::LogCategory ResidencyLog()
    {
        static const auto category = LLUtils::Logger::RegisterCategory("ImageResidency");
        return category;
    }
}  // namespace

namespace OIV
{

    bool ImageResidencyCacheProcessor::ProcessResidencyRequest(const ImageResidencyCacheKey& key,
                                                               ImageResidencyCacheValue& outValue)
    {
        using namespace IMCodec;

        auto res = fImageLoader.Decode(std::get<0>(key), ImageLoadFlags::None, {},
                                       PluginTraverseMode::AnyFileType | PluginTraverseMode::AnyPlugin, outValue);
        return res == ImageResult::Success;
    }

    ImageResidencyCache::ImageResidencyCache() : ImageResidencyCache(std::make_unique<ImageResidencyCacheProcessor>())
    {
    }

    ImageResidencyCache::ImageResidencyCache(std::unique_ptr<RequestProcessorType> processor, size_t threadCount)
        : fResidencyProcessor(processor != nullptr ? std::move(processor)
                                                   : std::make_unique<ImageResidencyCacheProcessor>()),
          fResidentCache(fResidencyProcessor.get()),
          fTaskExecutor(GetWorkerCount(threadCount),
                        [this](const ImageResidencyCacheTaskRequest& request) { return ProcessTask(request); })
    {
    }

    size_t ImageResidencyCache::GetWorkerCount(size_t threadCount)
    {
        if (threadCount != 0)
            return threadCount;

        const unsigned int defaultThreadCount = std::thread::hardware_concurrency();
        return defaultThreadCount == 0 ? 1 : defaultThreadCount;
    }

    TicketID ImageResidencyCache::SubmitTask(const ImageResidencyCacheKey& key, std::uint64_t version)
    {
        auto id = LLUtils::OperationScope::Current();
        if (!id.value)
            id = LLUtils::OperationId::Create();
        TicketID task      = fTaskExecutor.Submit(ImageResidencyCacheTaskRequest{key, version, id}, 0);
        fPendingTasks[key] = PendingTask{task, version, id};
        return task;
    }

    ImageResidencyCacheValue ImageResidencyCache::ProcessTask(const ImageResidencyCacheTaskRequest& request)
    {
        const LLUtils::OperationScope operation(request.operationId);
        LL_LOG(ResidencyLog(), LLUtils::LogLevel::Debug, "Decode request version {}", request.version);
        ImageResidencyCacheValue value{};
        const bool requestSucceeded = fResidencyProcessor->ProcessResidencyRequest(request.key, value);

        LL_LOG(ResidencyLog(), requestSucceeded ? LLUtils::LogLevel::Debug : LLUtils::LogLevel::Warning,
               "Decode version {} completed: {}", request.version, requestSucceeded);
        std::lock_guard lock(fMutex);
        const auto pendingTaskIt    = fPendingTasks.find(request.key);
        const bool isCurrentRequest = pendingTaskIt != fPendingTasks.end() &&
                                      pendingTaskIt->second.version == request.version;
        if (isCurrentRequest)
        {
            if (requestSucceeded)
                fResidentCache.SetResident(request.key, value);
            else
                fResidentCache.SetFailure(request.key);

            fPendingTasks.erase(pendingTaskIt);
        }

        return requestSucceeded ? value : ImageResidencyCacheValue{};
    }

    TicketID ImageResidencyCache::requestResidencyAsync(const LLUtils::native_string_type& filePath,
                                                        ImageResidencyCacheItemType itemType)
    {
        const ImageResidencyCacheKey key{filePath, itemType};

        std::unique_lock lock(fMutex);
        if (fResidentCache.HasResident(key))
            return TicketID::FromValue(fResidentCache.GetResident(key));

        auto taskIt = fPendingTasks.find(key);
        if (taskIt != fPendingTasks.end())
        {
            const auto sharedId = taskIt->second.operationId;
            const auto task     = taskIt->second.task;
            lock.unlock();
            LL_LOG(ResidencyLog(), LLUtils::LogLevel::Debug, "Request associated with shared decode operation {}",
                   sharedId.value);
            return task;
        }

        fResidentCache.SetPending(key);
        return SubmitTask(key, ++fNextRequestVersion);
    }

    void ImageResidencyCache::removeResidency(const LLUtils::native_string_type& filePath,
                                              ImageResidencyCacheItemType itemType)
    {
        const ImageResidencyCacheKey key{filePath, itemType};

        std::lock_guard lock(fMutex);
        fResidentCache.Clear(key);
        fPendingTasks.erase(key);
    }

    ResidencyState ImageResidencyCache::getResidencyState(const LLUtils::native_string_type& filePath,
                                                          ImageResidencyCacheItemType itemType) const
    {
        const ImageResidencyCacheKey key{filePath, itemType};

        std::lock_guard lock(fMutex);
        if (fPendingTasks.contains(key))
            return ResidencyState::Pending;

        return fResidentCache.GetResidencyState(key);
    }

}  // namespace OIV
