#include <OIVShared/ImageResidency.h>

namespace OIV
{

    bool ImageResidencyProcessor::ProcessResidencyRequest(const ImageResidencyKey& key,
                                                          ImageResidencyValue& outValue)
    {
        using namespace IMCodec;

        auto res = fImageLoader.Decode(std::get<0>(key), ImageLoadFlags::None, {},
                                       PluginTraverseMode::AnyFileType | PluginTraverseMode::AnyPlugin, outValue);
        return res == ImageResult::Success;
    }

    ImageResidency::ImageResidency()
        : ImageResidency(std::make_unique<ImageResidencyProcessor>())
    {
    }

    ImageResidency::ImageResidency(std::unique_ptr<RequestProcessorType> processor, size_t threadCount)
        : fResidencyProcessor(processor != nullptr ? std::move(processor) : std::make_unique<ImageResidencyProcessor>())
        , fResidentCache(fResidencyProcessor.get())
        , fTaskExecutor(GetWorkerCount(threadCount),
                        [this](const ImageResidencyTaskRequest& request) { return ProcessTask(request); })
    {
    }

    size_t ImageResidency::GetWorkerCount(size_t threadCount)
    {
        if (threadCount != 0)
            return threadCount;

        const unsigned int defaultThreadCount = std::thread::hardware_concurrency();
        return defaultThreadCount == 0 ? 1 : defaultThreadCount;
    }

    TicketID ImageResidency::SubmitTask(const ImageResidencyKey& key, std::uint64_t version)
    {
        TicketID task = fTaskExecutor.Submit(ImageResidencyTaskRequest{key, version}, 0);
        fPendingTasks[key] = PendingTask{task, version};
        return task;
    }

    ImageResidencyValue ImageResidency::ProcessTask(const ImageResidencyTaskRequest& request)
    {
        ImageResidencyValue value{};
        const bool requestSucceeded = fResidencyProcessor->ProcessResidencyRequest(request.key, value);

        std::lock_guard lock(fMutex);
        const auto pendingTaskIt = fPendingTasks.find(request.key);
        const bool isCurrentRequest =
            pendingTaskIt != fPendingTasks.end() && pendingTaskIt->second.version == request.version;
        if (isCurrentRequest)
        {
            if (requestSucceeded)
                fResidentCache.SetResident(request.key, value);
            else
                fResidentCache.SetFailure(request.key);

            fPendingTasks.erase(pendingTaskIt);
        }

        return requestSucceeded ? value : ImageResidencyValue{};
    }

    TicketID ImageResidency::requestResidencyAsync(const LLUtils::native_string_type& filePath,
                                                   ImageResidencyItemType itemType)
    {
        const ImageResidencyKey key{filePath, itemType};

        std::lock_guard lock(fMutex);
        if (fResidentCache.HasResident(key))
            return TicketID::FromValue(fResidentCache.GetResident(key));

        auto taskIt = fPendingTasks.find(key);
        if (taskIt != fPendingTasks.end())
            return taskIt->second.task;

        fResidentCache.SetPending(key);
        return SubmitTask(key, ++fNextRequestVersion);
    }

    void ImageResidency::removeResidency(const LLUtils::native_string_type& filePath, ImageResidencyItemType itemType)
    {
        const ImageResidencyKey key{filePath, itemType};

        std::lock_guard lock(fMutex);
        fResidentCache.Clear(key);
        fPendingTasks.erase(key);
    }

    ResidencyState ImageResidency::getResidencyState(const LLUtils::native_string_type& filePath,
                                                     ImageResidencyItemType itemType) const
    {
        const ImageResidencyKey key{filePath, itemType};

        std::lock_guard lock(fMutex);
        if (fPendingTasks.contains(key))
            return ResidencyState::Pending;

        return fResidentCache.GetResidencyState(key);
    }

}  // namespace OIV
