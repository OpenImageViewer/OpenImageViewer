#include <OIVAppCore/ViewerSession.h>
#include <OIVShared/ViewTransformController.h>
#include <OIVAppCore/ViewActionController.h>
#include <OIVAppCore/ColorCountPolicy.h>
#include <OIVShared/PixelHelper.h>
#include <LLUtils/Thread.h>
#include <algorithm>
#include <LLUtils/Exception.h>
namespace OIV
{
    ViewerSession::ViewerSession(IFileWatcher* watcher, ImageResidencyCache* residency, PostTask post)
        : fFileWatcher(watcher), fImageResidencyCache(residency), fPostTask(std::move(post)),
          fImageOpenController(
              std::make_unique<ImageOpenController>(std::make_unique<OIVImageFileLoader>(fImageLoader)))
    {
    }

    ViewerSession::ViewerSession(std::unique_ptr<IFileWatcher> watcher, PostTask post)
        : ViewerSession(watcher.get(), nullptr, std::move(post))
    {
        fOwnedWatcher = std::move(watcher);
        fOwnedResidency.emplace();
        fImageResidencyCache = &*fOwnedResidency;
    }

    bool ViewerSession::ApplySetting(const AppSettingsPolicy::Action& action)
    {
        switch (action.type)
        {
            case AppSettingsPolicy::ActionType::MaxZoom:
                fMaxPixelSize = action.floatValue;
                break;
            case AppSettingsPolicy::ActionType::ImageMarginX:
                fImageMargins.x = action.floatValue;
                break;
            case AppSettingsPolicy::ActionType::ImageMarginY:
                fImageMargins.y = action.floatValue;
                break;
            case AppSettingsPolicy::ActionType::MinImageSize:
                fMinImageSize = action.floatValue;
                break;
            case AppSettingsPolicy::ActionType::DefaultSortMode:
                fFileSorter.SetSortType(action.sortType);
                break;
            case AppSettingsPolicy::ActionType::SortDirection:
                fFileSorter.SetSortDirection(action.sortType, action.sortDirection);
                break;
            case AppSettingsPolicy::ActionType::BiggestSubImageOnLoad:
                fDisplayBiggestSubImageOnLoad = action.boolValue;
                break;
            default:
                return false;
        }
        return true;
    }

    ViewerSession::~ViewerSession()
    {
        // Stop owned or injected producers before callbacks, queued CPU results, and controller references
        // are destroyed. Surface teardown deliberately does not stop session work.
        fStopping = true;
        fLifetime.reset();
        if (fFileWatcher)
            fFileWatcher->StopNotifications();
        if (fImageResidencyCache)
            fImageResidencyCache->Stop();
        if (fCountingColorsThread.joinable())
            fCountingColorsThread.join();
    }

    void ViewerSession::Attach(std::function<void()> wake)
    {
        if (!wake)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "A completion notification is required");
        {
            const std::scoped_lock lock(fCompletionMutex);
            if (fCompletionWake)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "A session already has an attached viewer");
            fCompletionWake = std::make_shared<std::function<void()>>(std::move(wake));
        }
        ScheduleCompletions();
    }
    void ViewerSession::Detach()
    {
        const std::scoped_lock lock(fCompletionMutex);
        fCompletionWake = {};
    }
    void ViewerSession::QueueCompletion(CompletionPayload payload)
    {
        if (fStopping)
            return;
        {
            const std::scoped_lock lock(fCompletionMutex);
            fCompletions.push_back({std::move(payload), LLUtils::OperationScope::Current()});
        }
        ScheduleCompletions();
    }
    void ViewerSession::ScheduleCompletions()
    {
        {
            const std::scoped_lock lock(fCompletionMutex);
            if (fStopping || !fPostTask || !fCompletionWake || fCompletions.empty() || fWakeScheduled)
                return;
            fWakeScheduled = true;
        }
        // Never call the scheduler under the queue mutex. Host dispatch may run nested native messages.
        if (!fPostTask(
                [this, lifetime = fWeakLifetime]
                {
                    if (lifetime.expired())
                        return;
                    std::shared_ptr<std::function<void()>> wake;
                    {
                        const std::scoped_lock lock(fCompletionMutex);
                        fWakeScheduled = false;
                        wake           = fCompletionWake;
                    }
                    if (wake)
                        (*wake)();
                }))
        {
            // Failed posting owns no callback. Retain the batch so another producer or attachment can retry.
            const std::scoped_lock lock(fCompletionMutex);
            fWakeScheduled = false;
        }
    }
    std::vector<ViewerCompletion> ViewerSession::TakeCompletions()
    {
        std::vector<ViewerCompletion> result;
        const std::scoped_lock lock(fCompletionMutex);
        result.swap(fCompletions);
        return result;
    }
    void ViewerSession::InitializeBrowsing()
    {
        if (fBrowseSessionController || !fImageResidencyCache)
            return;
        if (fFileWatcher)
            fFileWatchSubscription = fFileWatcher->GetFileChangedEvent().Subscribe([this](const auto& event)
                                                                                   { QueueCompletion(event); });
        fBrowseSessionController = std::make_unique<BrowseSessionController>(
            fFileWatcher, &fFileSorter, fKnownFileTypesSet, fKnownFileTypes, *fImageResidencyCache,
            [this](const auto& file, auto pixels)
            { QueueCompletion(FileIndexResidencyReadyData{file, std::move(pixels)}); },
            [this](const auto& result) { QueueCompletion(result); });
        fImageOpenController->SetBrowseSessionController(fBrowseSessionController.get());
        // Only a newly created controller needs the initial file. Recommitting on presenter replacement
        // cancels pending navigation before retained candidate completions can be delivered.
        if (const auto* image = fImageState.GetOpenedImage(); image && image->source == ImageSource::File)
            (void) fBrowseSessionController->CommitCurrentFile(image->path);
    }

    uint32_t ViewerSession::StartAnimation(SequencerPolicy::Clock::time_point now)
    {
        uint32_t interval = 0;
        if (const auto* image = fImageState.GetOpenedImage();
            image && image->pixels->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame &&
            image->pixels->GetNumSubImages() != 0)
        {
            // Frame zero is already selected. Time its full duration before advancing to frame one.
            fCurrentFrame = SequencerPolicy::NextFrame(0, image->pixels->GetNumSubImages());
            interval = fSequencerPolicy.StartFrame(image->pixels->GetSubImage(0)->GetAnimationData().delayMilliseconds,
                                                   now);
        }
        return interval;
    }

    ViewerSession::AnimationStep ViewerSession::AdvanceAnimation(SequencerPolicy::Clock::time_point now)
    {
        if (const auto remaining = fSequencerPolicy.RemainingFrameMs(now); remaining != 0)
            return {remaining, false};
        const auto& image = fImageState.GetOpenedImage()->pixels;
        const auto frame  = image->GetSubImage(fCurrentFrame);
        fImageState.SetSelectedSource(frame);
        const auto interval = fSequencerPolicy.StartFrame(frame->GetAnimationData().delayMilliseconds, now);
        fCurrentFrame       = SequencerPolicy::NextFrame(fCurrentFrame, image->GetNumSubImages());
        return {interval, true};
    }

    void ViewerSession::RequestColorCount()
    {
        if (fImageState.GetSelectedSource() && fImageState.GetColorCountState() == ColorCountState::NotRequested &&
            !fIsColorThreadRunning)
        {
            if (fCountingColorsThread.joinable())
                fCountingColorsThread.join();
            const auto generation = fImageState.GetSourceGeneration();
            auto pixels           = fImageState.GetSelectedSource();
            fCountingColorsThread = LLUtils::StartThread(
                [this](IMCodec::ImageSharedPtr image, uint64_t sourceGeneration)
                { QueueCompletion(CountColorsData{sourceGeneration, PixelHelper::CountUniqueValues(image)}); },
                std::move(pixels), generation);
            fIsColorThreadRunning = true;
            fImageState.SetColorCountPending();
        }
    }

    bool ViewerSession::CompleteColorCount(const CountColorsData& result, bool countCurrentSource)
    {
        // Finish worker bookkeeping even when the source changed while this count was running.
        fIsColorThreadRunning = false;
        const auto action     = ColorCountPolicy::DecideCompletion(result.generation, fImageState.GetSourceGeneration(),
                                                                   countCurrentSource);
        if (action == ColorCountCompletionAction::ApplyToCurrentImage)
            fImageState.CompleteColorCount(ColorCountPolicy::NormalizeCountResult(
                result.colorCount, UniqueColorsUninitialized - 1, UniqueColorsFailed));
        else if (action == ColorCountCompletionAction::CountVisibleImage)
            RequestColorCount();
        return action == ColorCountCompletionAction::ApplyToCurrentImage;
    }

    void ViewerSession::SetViewport(LLUtils::PointI32 size)
    {
        fImageState.SetViewportSize(size);
    }
    LLUtils::PointF64 ViewerSession::ImageSize(ImageSizeType type) const
    {
        if (type == ImageSizeType::Visible)
            return fImageState.GetVisibleSize();
        if (type != ImageSizeType::Original && type != ImageSizeType::Transformed)
            LL_EXCEPTION_UNEXPECTED_VALUE;
        const auto stage   = type == ImageSizeType::Original ? ImageProcessingStage::Source
                                                             : ImageProcessingStage::GeometricTransform;
        const auto& pixels = fImageState.GetRetainedImage(stage);
        return pixels ? static_cast<LLUtils::PointF64>(pixels->GetDimensions()) : LLUtils::PointF64{};
    }
    LLUtils::PointF64 ViewerSession::ResolveOffset(LLUtils::PointF64 offset) const
    {
        return ViewTransformController::ResolveOffset(offset,
                                                      static_cast<LLUtils::PointF64>(fImageState.GetViewportSize()),
                                                      fImageState.GetVisibleSize(), fImageMargins);
    }
    void ViewerSession::SetOffset(LLUtils::PointF64 offset, bool preserveLock)
    {
        fImageState.SetOffset(ResolveOffset(offset));
        if (!preserveLock)
            fIsOffsetLocked = false;
    }
    LLUtils::PointF64 ViewerSession::CenterOffset(LLUtils::PointF64 center) const
    {
        return ViewTransformController::CenterOffset(center, fImageState.GetVisibleSize());
    }
    bool ViewerSession::ZoomTo(double scale, LLUtils::PointI32 point, LLUtils::PointF64 center, bool preserveFit)
    {
        if (!fIsLockFitToScreen)
            scale = ViewActionController::ResolveZoomValue(
                scale, false,
                ViewActionController::MinimumPixelSize(fMinImageSize, ImageSize(ImageSizeType::Transformed)),
                fMaxPixelSize);
        if (scale == fImageState.GetScale().x)
            return false;
        const auto clientPoint = ViewActionController::ResolveZoomPoint(point, center);
        const auto imagePoint  = ViewTransformController::ClientToImage(static_cast<LLUtils::PointF64>(clientPoint),
                                                                        fImageState.GetScale().x,
                                                                        fImageState.GetOffset());
        const auto delta       = ViewTransformController::ZoomOffset(imagePoint, fImageState.GetScale().x, scale);
        fImageState.SetResample(false);
        fImageState.SetScale(scale);
        fImageState.Refresh();
        SetOffset(fImageState.GetOffset() + delta,
                  ViewActionController::ShouldPreserveOffsetLockForZoom(point.x, point.y));
        if (!preserveFit)
            fIsLockFitToScreen = false;
        return true;
    }

    double ViewerSession::FitScale() const
    {
        const double scale = ViewTransformController::FitScale(
            static_cast<LLUtils::PointF64>(fImageState.GetViewportSize()), ImageSize(ImageSizeType::Transformed));
        return fFitLargerOnly ? std::min(scale, 1.0) : scale;
    }
}  // namespace OIV
