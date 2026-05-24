#pragma once

#include <OIVShared/ResidentCache.h>
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
#include <OIVShared/PriorityTaskExecutor.h>

namespace OIV
{
    enum class ImageResidencyCacheItemType
    {
        None       = 0 << 0,
        Icon32x32  = 1 << 0,
        Icon64x64  = 1 << 1,
        Thumbnail  = 2 << 1,
        ScreenSize = 3 << 1,
        FullSize   = 4 << 1
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(ImageResidencyCacheItemType)

    struct ImageResidencyCacheParameters
    {
        bool traverseAllFileTypes = false;
    };

    using ImageResidencyCacheKey   = std::tuple<LLUtils::native_string_type, ImageResidencyCacheItemType>;
    using ImageResidencyCacheValue = IMCodec::ImageSharedPtr;
    using ResidentCacheType        = ResidentCache<ImageResidencyCacheKey, ImageResidencyCacheValue>;
    using RequestProcessorType     = ResidentCacheType::IRequestProcessor;

    struct ImageResidencyCacheTaskRequest
    {
        ImageResidencyCacheKey key;
        std::uint64_t version = 0;
    };

    using TaskExecutorType = PriorityTaskExecutor<ImageResidencyCacheTaskRequest, ImageResidencyCacheValue>;
    using TicketID         = TaskExecutorType::TaskType;

    class ImageResidencyCacheProcessor : public RequestProcessorType
    {
      public:

        bool ProcessResidencyRequest(const ImageResidencyCacheKey& key, ImageResidencyCacheValue& outValue) override;

      private:

        IMCodec::ImageLoader fImageLoader;
    };

    class ImageResidencyCache
    {
      public:

        ImageResidencyCache();
        explicit ImageResidencyCache(std::unique_ptr<RequestProcessorType> processor, size_t threadCount = 0);
        TicketID requestResidencyAsync(const LLUtils::native_string_type& filePath,
                                       ImageResidencyCacheItemType itemType);
        void removeResidency(const LLUtils::native_string_type& filePath, ImageResidencyCacheItemType itemType);
        ResidencyState getResidencyState(const LLUtils::native_string_type& filePath,
                                         ImageResidencyCacheItemType itemType) const;

      private:

        struct PendingTask
        {
            TicketID task;
            std::uint64_t version = 0;
        };

        static size_t GetWorkerCount(size_t threadCount);
        TicketID SubmitTask(const ImageResidencyCacheKey& key, std::uint64_t version);
        ImageResidencyCacheValue ProcessTask(const ImageResidencyCacheTaskRequest& request);

        std::unique_ptr<RequestProcessorType> fResidencyProcessor;
        ResidentCacheType fResidentCache;
        std::map<ImageResidencyCacheKey, PendingTask> fPendingTasks;
        std::uint64_t fNextRequestVersion = 0;
        mutable std::mutex fMutex;
        TaskExecutorType fTaskExecutor;
    };

}  // namespace OIV
