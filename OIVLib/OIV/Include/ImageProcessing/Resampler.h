#pragma once
#include <cstdint>
#include <vector>
#include <thread>
#if RESAMPLE_THREAD_POOL
    #include <condition_variable>
    #include <functional>
    #include <mutex>
#endif
#include <Image.h>
#include <LLUtils/Point.h>

namespace OIV
{

    // Reusable CPU producer with cached worker/task storage and no renderer dependency.
    class Resampler
    {
      public:

        // Synchronously resample packed 32-bit render pixels using the existing policy.
        // Input pixels remain read-only until the reused workers have joined.
        IMCodec::ImageSharedPtr Resample(IMCodec::ImageSharedPtr sourceImage, LLUtils::PointI32 targetSize);

      private:

        // Source sampling bounds relative to the current source texel.
        struct ResamplerBox
        {
            // Top source-neighborhood offset in texels.
            int32_t top;
            // Left source-neighborhood offset in texels.
            int32_t left;
            // Bottom source-neighborhood offset in texels.
            int32_t bottom;
            // Right source-neighborhood offset in texels.
            int32_t right;
        };

        // Borrowed packed 32-bit source and writable target buffers for one CPU operation.
        struct ResamplerParams
        {
            // Writable packed target pixels, borrowed for the duration of processing.
            uint32_t* targetBuffer;
            // Target width in texels.
            uint32_t targetWidth;
            // Target height in rows.
            uint32_t targetHeight;
            // Read-only packed source pixels, borrowed for processing.
            const uint32_t* sourceBuffer;
            // Source width in texels.
            uint32_t sourceWidth;
            // Source height in rows.
            uint32_t sourceHeight;
        };

        // Source neighborhood used by the existing averaging algorithm.
        struct AverageParams
        {
            // Read-only source pixels used by neighborhood averaging.
            const uint32_t* imageBuffer;
            // Source width in texels.
            size_t ImageWidth;
            // Source height in rows.
            size_t ImageHeight;
            // Current source column in texels.
            size_t ImageX;
            // Current source row in texels.
            size_t ImageY;
            // Neighborhood bounds relative to the current sample.
            ResamplerBox box;
        };

        // Worker slice sharing borrowed buffers until the synchronous operation joins.
        struct ResampleTask
        {
            // Worker index selecting its contiguous target-row slice.
            size_t TaskID;
            // Borrowed operation buffers and dimensions shared by workers.
            ResamplerParams resampleParams;
            // Neighborhood bounds relative to the current sample.
            ResamplerBox box;
            // Number of target texels distributed across workers.
            size_t totalTargetTexels;
            // Source texels per target texel along the horizontal axis.
            double ratioX;
            // Source texels per target texel along the vertical axis.
            double ratioY;
            // Number of worker slices used by this operation.
            size_t totalThreads;

#if RESAMPLE_THREAD_POOL
            // Borrowed condition variable for the existing optional thread-pool mode.
            std::condition_variable* cv;
            // Borrowed mutex protecting the optional thread-pool notification.
            std::mutex* mtx;
            // Borrowed completion callback for the optional thread-pool mode.
            std::function<void()>* Callback;
#endif
        };

        void Resample(const ResamplerParams& params);
        void Init();
        uint32_t GetAverageAt(const AverageParams& params);
        void ResampleThreadEntryPoint(ResampleTask* task);

      private:

        // Reused thread slots, joined before synchronous processing returns.
        std::vector<std::thread> fThreads;
        // Reused task slots, valid until the associated workers finish.
        std::vector<ResampleTask> fTasks;
        // Cached existing memory-operation thread count.
        uint32_t fNumOfIdealThreadsForResampling = 1;
        // Whether thread/task storage has been initialized.
        bool fInitialized = false;
    };
}  // namespace OIV
