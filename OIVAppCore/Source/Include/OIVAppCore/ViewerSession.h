#pragma once
#include <OIVAppCore/ImageState.h>
#include <RendererCore/RenderTypes.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/SlideshowPolicy.h>
#include <OIVAppCore/ImageOpenController.h>
#include <OIVAppCore/ViewerCompletion.h>
#include <OIVAppCore/AppSettingsPolicy.h>
#include <OIVShared/FileSorter.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>
namespace OIV
{
    enum class ImageSizeType
    {
        Original,
        Transformed,
        Visible
    };

    // Determines whether to change zoom / pan when loading a new file
    enum class ResetTransformationMode
    {
        DoNothing = 0,
        ResetAll  = 1,
        Count
    };

    enum class DownscalingTechnique
    {
        None,
        HardwareMipmaps,
        Software,
        Count
    };

    class Viewer;
    // One independent presentation on its owning UI thread. CPU pixels, view choices, and image-space selection
    // survive surface replacement. Native gestures, timers, GPU registrations, and windows do not live here.
    // ImageState is the sole owner of zoom, offset, source generations, and processing caches.
    class ViewerSession final
    {
      public:

        using PostTask = std::function<bool(std::move_only_function<void()>)>;
        // Production sessions own their producers. PostTask enqueues on the UI thread and never runs inline.
        ViewerSession(std::unique_ptr<IFileWatcher> watcher, PostTask post);
        // Injected services outlive the session and are stopped during its teardown. With no scheduler,
        // presenters may drain typed completions explicitly.
        ViewerSession(IFileWatcher* watcher = nullptr, ImageResidencyCache* residency = nullptr, PostTask post = {});
        ~ViewerSession();
        ViewerSession(const ViewerSession&)            = delete;
        ViewerSession& operator=(const ViewerSession&) = delete;
        // Surface attachment changes only completion notification. Pending results remain session-owned while
        // detached and are applied when a presenter next drains them, preserving the established UI mutation order.
        void Attach(std::function<void()> completionWake);
        void Detach();
        void QueueCompletion(CompletionPayload payload);
        std::vector<ViewerCompletion> TakeCompletions();
        void InitializeBrowsing();
        // Apply retained settings that need no native updates. Return false when the presenter must
        // handle a setting with rendering or timer side effects.
        bool ApplySetting(const AppSettingsPolicy::Action& action);
        IMCodec::ImageLoader& Loader() { return fImageLoader; }
        const auto& KnownFileTypes() const { return fKnownFileTypesSet; }
        const ImageState& Image() const { return fImageState; }
        const LLUtils::RectI32& Selection() const { return fImageSpaceSelection; }
        // Accept a successfully decoded CPU image. Preparing processing stages remains explicit.
        void OpenImage(ViewerImage image)
        {
            fCurrentFrame = 0;
            fSequencerPolicy.Reset(image.pixels);
            fImageState.SetOpenedImage(std::move(image));
            fImageState.ResetUserState();
        }
        struct AnimationStep
        {
            uint32_t intervalMs;
            bool changed;
        };
        // Timing and frame selection survive presenter replacement. Only the frontend owns the timer.
        uint32_t StartAnimation(SequencerPolicy::Clock::time_point now = SequencerPolicy::Clock::now());
        // Requires an active animation started by StartAnimation. Early ticks retain the selected frame.
        AnimationStep AdvanceAnimation(SequencerPolicy::Clock::time_point now = SequencerPolicy::Clock::now());
        // The worker retains its source pixels and posts a typed result even while no presenter is attached.
        void RequestColorCount();
        // A stale result may start a new count if a consumer still requests it. Returns true when applied.
        bool CompleteColorCount(const CountColorsData& result, bool countCurrentSource);
        void SetSelection(LLUtils::RectI32 selection) { fImageSpaceSelection = selection; }
        void SetViewport(LLUtils::PointI32 size);
        LLUtils::PointF64 ImageSize(ImageSizeType type) const;
        LLUtils::PointF64 ResolveOffset(LLUtils::PointF64 offset) const;
        void SetOffset(LLUtils::PointF64 offset, bool preserveLock = false);
        LLUtils::PointF64 CenterOffset(LLUtils::PointF64 center) const;
        double FitScale() const;
        // Requires prepared source geometry. Coordinates are viewport pixels, already normalized by the frontend.
        bool ZoomTo(double scale, LLUtils::PointI32 point, LLUtils::PointF64 center, bool preserveFit = false);
        void Refresh() { fImageState.Refresh(); }

      private:

        // GUI orchestration preserves the established processing order without exposing its mutable settings
        // as public API or adding one forwarding interface per setting.
        friend class Viewer;
        void ScheduleCompletions();
        std::unique_ptr<IFileWatcher> fOwnedWatcher;
        std::optional<ImageResidencyCache> fOwnedResidency;
        IFileWatcher::FolderID fConfigurationFolderID{};
        IFileWatcher* fFileWatcher{};
        ImageResidencyCache* fImageResidencyCache{};
        PostTask fPostTask;
        struct Lifetime
        {
        };
        std::shared_ptr<Lifetime> fLifetime{std::make_shared<Lifetime>()};
        const std::weak_ptr<Lifetime> fWeakLifetime{fLifetime};
        std::atomic_bool fStopping{};
        std::mutex fCompletionMutex;
        std::vector<ViewerCompletion> fCompletions;
        std::shared_ptr<std::function<void()>> fCompletionWake;
        bool fWakeScheduled{};
        IMCodec::ImageLoader fImageLoader;
        std::unique_ptr<ImageOpenController> fImageOpenController;
        FileSorter fFileSorter;
        std::set<LLUtils::native_string_type> fKnownFileTypesSet;
        LLUtils::native_string_type fKnownFileTypes;
        LLUtils::native_string_type fPendingFolderLoad;
        std::atomic_bool fIsColorThreadRunning = false;
        std::thread fCountingColorsThread;
        IFileWatcher::OnFileChangedEventArgsEvent::Subscription fFileWatchSubscription;
        std::unique_ptr<BrowseSessionController> fBrowseSessionController;
        bool fFitLargerOnly{};
        OIV::FilterType fImageFilter = OIV::FilterType::None;
        double fMaxPixelSize         = 30.0;
        double fMinImageSize         = 150.0;
        SlideshowPolicy fSlideshowPolicy;
        int fCurrentFrame = 0;
        SequencerPolicy fSequencerPolicy;
        bool fIsGridEnabled                      = false;
        OIV::TransparencyShade fTransparencyMode = OIV::TransparencyShade::Medium;
        LLUtils::RectI32 fImageSpaceSelection    = LLUtils::RectI32::Zero;
        bool fDisplayBiggestSubImageOnLoad       = true;
        bool fIsOffsetLocked                     = false;
        bool fIsLockFitToScreen                  = false;
        LLUtils::PointF64 fImageMargins{0.75, 0.75};
        ResetTransformationMode fResetTransformationMode = ResetTransformationMode::ResetAll;
        const OIV::ColorExposure DefaultColorCorrection  = {1.0, 0.0, 1.0, 1.0, 1.0};
        OIV::ColorExposure fColorExposure                = DefaultColorCorrection;
        OIV::ColorExposure fLastColorExposure            = fColorExposure;
        ImageState fImageState;
        DownscalingTechnique fDownScalingTechnique = DownscalingTechnique::Software;
    };
}  // namespace OIV
