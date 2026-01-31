#pragma once

#include <oivshared/ResidentCache.h>
#include <LLUtils/EnumClassBitwise.h>
#include <LLUtils/StringDefs.h>
#include <IImagePlugin.h>
#include <ImageLoader.h>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <oivshared/PriorityTaskExecutor.h>

namespace OIV
{
    enum class ImageResidencyItemType
    {
        None       = 0 << 0,
        Icon32x32  = 1 << 0,
        Icon64x64  = 1 << 1,
        Thumbnail  = 2 << 1,
        ScreenSize = 3 << 1,
        FullSize   = 4 << 1
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(ImageResidencyItemType)

    struct ImageResidencyParameters
    {
        bool traverseAllFileTypes = false;
    };

    using ImageResidencyKey    = std::tuple<LLUtils::native_string_type, ImageResidencyItemType>;
    using ImageResidencyValue  = IMCodec::ImageSharedPtr;
    using ResidentCacheType    = ResidentCache<ImageResidencyKey, ImageResidencyValue>;
    using RequestProcessorType = ResidentCacheType::IRequestProcessor;

    struct ImageResidencyTaskRequest
    {
        ImageResidencyKey key;
        std::uint64_t version = 0;
    };

    using TaskExecutorType     = PriorityTaskExecutor<ImageResidencyTaskRequest, ImageResidencyValue>;
    using TicketID             = TaskExecutorType::TaskType;
   
    class ImageResidencyProcessor : public RequestProcessorType
    {
      public:
        bool ProcessResidencyRequest(const ImageResidencyKey& key, ImageResidencyValue& outValue) override;

      private:
        IMCodec::ImageLoader fImageLoader;
    };

    class ImageResidency
    {
      public:

        ImageResidency();
        explicit ImageResidency(std::unique_ptr<RequestProcessorType> processor, size_t threadCount = 0);
        TicketID requestResidencyAsync(
            const LLUtils::native_string_type& filePath,
            ImageResidencyItemType itemType);
        void removeResidency(const LLUtils::native_string_type& filePath, ImageResidencyItemType itemType);
        ResidencyState getResidencyState(const LLUtils::native_string_type& filePath,
                                         ImageResidencyItemType itemType) const;

      private:
        struct PendingTask
        {
            TicketID task;
            std::uint64_t version = 0;
        };

        static size_t GetWorkerCount(size_t threadCount);
        TicketID SubmitTask(const ImageResidencyKey& key, std::uint64_t version);
        ImageResidencyValue ProcessTask(const ImageResidencyTaskRequest& request);

        std::unique_ptr<RequestProcessorType> fResidencyProcessor;
        ResidentCacheType fResidentCache;
        std::map<ImageResidencyKey, PendingTask> fPendingTasks;
        std::uint64_t fNextRequestVersion = 0;
        mutable std::mutex fMutex;
        TaskExecutorType fTaskExecutor;

    };

}  // namespace OIV
