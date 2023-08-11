#pragma once
#include "../LabelManager.h"
#include "../RecursiveDelayOp.h"
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/EnumClassBitwise.h>
#include <LLUtils/StopWatch.h>
#include <Win32/Timer.h>

namespace OIV
{
    enum class MessageFlags
    {
          None              = 0 << 0
        , Persistent        = 1 << 0 // message can not be replaced by another message.
        , Interchangeable   = 1 << 1 // message can be replaced by another message in the same group
        , Moveable          = 1 << 2 // message moves to the top when replaced, implies 'Interchangeable'
        , ManualRemove      = 1 << 3 // message stays on screen until user request removal
    };


    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(MessageFlags);

    using GroupID = uint32_t;

    enum class DisplayStage
    {
         Hidden
        ,Visible
        ,Fading
    };


    struct MessageData
    {
        DisplayStage displayStage{};
        uint32_t ttl; //Time to live in milliseconds
        int64_t timeStamp;
        GroupID groupID;
        MessageFlags flags;
        uint16_t messageUniqueID;
        OIVTextImage* message;
    };

    using RequestRefreshCallbackType = std::function<void()>;

	class MessageManager
	{
    public:
        MessageManager(HWND associatedtimerWindow, LabelManager* labelManager, size_t maxMessages, RequestRefreshCallbackType callback);
        void SetUserMessage(uint32_t groupID, MessageFlags commandGroup, const std::wstring& message);
        void UpdateMessagesPosition();
        void RemoveGroup(GroupID groupID);

 
    private:

        using ListMessageData = std::list<MessageData>;
        MessageManager::ListMessageData::iterator RemoveElement(ListMessageData::iterator it);
        void OnTimer();
        ListMessageData::iterator FindVisible(GroupID groupID);
        void PushNextMessage(GroupID groupID, MessageFlags groupFlags, const std::wstring& message);
        void CreateMessageTemplate(MessageData& messageData);
        void UpdateMessage(const std::wstring& message, MessageData& messageData);
        void OnRefresh();
        void OnWindowSizeChange(const EventManager::SizeChangeEventParams& sizeChangedParams);
    private:
        HWND fWindow;
        LLUtils::UniqueIdProvider<uint16_t> fMessageIDProvider { 1 };
        LabelManager* fLabelManager{};
        size_t fMaxMessages{};
        ListMessageData fMessages;
        uint32_t fMinDelayRemoveMessage = 1000;
        uint32_t fDelayPerCharacter = 40;
        LLUtils::StopWatch fStopWatch{ true };
        ::Win32::Timer fTimerHideUserMessage;
        ::Win32::Timer fFadeTimer;
        RequestRefreshCallbackType fRefreshCallback;
        RecrusiveDelayedOp fRefreshRequest;
        int32_t fMaxMessageWidth{};
        int32_t fMarginLeft = 20;
        int32_t fMarginTop = 20;
        int32_t fMarginRight = 20;

	};
}