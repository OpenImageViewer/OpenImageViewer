#pragma once
#include <list>
#include "../LabelManager.h"
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/EnumClassBitwise.h>
#include <LLUtils/StopWatch.h>
#include <LWS/Timer.hpp>
#include <LWS/Window.hpp>
#include <OIVShared/RecursiveDelayOp.h>

namespace OIV
{
    enum class MessageFlags
    {
        None       = 0 << 0,
        Persistent = 1 << 0  // message can not be replaced by another message.
        ,
        Interchangeable = 1 << 1  // message can be replaced by another message in the same group
        ,
        Moveable = 1 << 2  // message moves to the top when replaced, implies 'Interchangeable'
        ,
        ManualRemove = 1 << 3  // message stays on screen until user request removal
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(MessageFlags);

    using GroupID = uint32_t;

    enum class DisplayStage
    {
        Hidden,
        Visible,
        Fading
    };

    // Scheduling state borrows a label until producer stop and label-manager cleanup.
    struct MessageData
    {
        // Current visibility/fade stage in the existing message policy.
        DisplayStage displayStage{};
        // Message lifetime in milliseconds.
        uint32_t ttl;
        // Creation time in milliseconds since the manager stopwatch epoch.
        int64_t timeStamp;
        // Application message group used by replacement and removal policy.
        GroupID groupID;
        // Existing persistence, replacement and manual-removal options.
        MessageFlags flags;
        // Message-local identity used to reuse named labels, distinct from render IDs.
        uint16_t messageUniqueID;
        // Borrowed label, cleared from scheduling state before labels are destroyed.
        TextLabel* message;
    };

    using RequestRefreshCallbackType = std::function<void()>;

    // Viewer message scheduling borrows labels and stops its producers before label cleanup.
    class MessageManager
    {
      public:

        MessageManager(LWS::Window& associatedWindow, LabelManager* labelManager, size_t maxMessages,
                       RequestRefreshCallbackType callback);
        // Stop label-dependent producers before LabelManager releases their targets.
        void Stop();
        void OnWindowSizeChange(const LWS::PixelSize& params);
        void SetUserMessage(uint32_t groupID, MessageFlags commandGroup, const LLUtils::native_string_type& message);
        void UpdateMessagesPosition();
        void RemoveGroup(GroupID groupID);

      private:

        using ListMessageData = std::list<MessageData>;
        MessageManager::ListMessageData::iterator RemoveElement(ListMessageData::iterator it);
        void OnTimer();
        ListMessageData::iterator FindVisible(GroupID groupID);
        void PushNextMessage(GroupID groupID, MessageFlags groupFlags, const LLUtils::native_string_type& message);
        void CreateMessageTemplate(MessageData& messageData);
        void UpdateMessage(const LLUtils::native_string_type& message, MessageData& messageData);
        void OnRefresh();

      private:

        // Existing message-local identity allocator for named label reuse.
        LLUtils::UniqueIdProvider<uint16_t> fMessageIDProvider{1};
        // Borrowed label owner, which outlives this scheduling object.
        LabelManager* fLabelManager{};
        // Maximum number of message scheduling entries retained.
        size_t fMaxMessages{};
        // Scheduled messages borrowing label pointers until Stop clears them.
        ListMessageData fMessages;
        // Minimum message lifetime in milliseconds.
        uint32_t fMinDelayRemoveMessage = 1000;
        // Additional lifetime per character in milliseconds.
        uint32_t fDelayPerCharacter = 40;
        // Epoch used for relative message timestamps.
        LLUtils::StopWatch fStopWatch{true};
        // Owned expiry timer detached before label release.
        LWS::Timer fTimerHideUserMessage;
        // Owned fade timer detached before label release.
        LWS::Timer fFadeTimer;
        // Viewer redraw request guarded by the viewer callback lifetime.
        RequestRefreshCallbackType fRefreshCallback;
        // Existing redraw coalescing within message updates.
        RecursiveDelayedOp fRefreshRequest;
        // Wrapping width in client pixels.
        int32_t fMaxMessageWidth{};
        // Left placement margin in client pixels.
        int32_t fMarginLeft = 20;
        // Top placement margin in client pixels.
        int32_t fMarginTop = 20;
        // Right wrapping margin in client pixels.
        int32_t fMarginRight = 20;
    };
}  // namespace OIV
