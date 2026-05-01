#include "MessageManager.h"

namespace OIV
{
    MessageManager::MessageManager(HWND associatedtimerWindow, LabelManager* labelManager, size_t maxMessages, RequestRefreshCallbackType callback) :
        fWindow(associatedtimerWindow), fLabelManager(labelManager), fMaxMessages(maxMessages), fRefreshCallback(callback)
        , fRefreshRequest(std::bind(&MessageManager::OnRefresh, this))
    {
        fTimerHideUserMessage.SetTargetWindow(fWindow);
        fTimerHideUserMessage.SetCallback(std::bind(&MessageManager::OnTimer, this));
        fFadeTimer.SetTargetWindow(fWindow);
        fFadeTimer.SetCallback(std::bind(&MessageManager::OnTimer, this));

        EventManager::GetSingleton().SizeChange.Add(std::bind(&MessageManager::OnWindowSizeChange, this, std::placeholders::_1));
    }

    void MessageManager::OnWindowSizeChange(const EventManager::SizeChangeEventParams& sizeChangedParams)
    {
        fMaxMessageWidth = sizeChangedParams.width - (fMarginLeft + fMarginRight);

        bool needrefresh = false;
        for (const auto& messageData : fMessages)
        {
            messageData.message->SetMaxWidth(fMaxMessageWidth);
            if (messageData.message->IsDirty())
                needrefresh |= true;
        }

        if (needrefresh)
        {

            fRefreshRequest.Begin();
            UpdateMessagesPosition();
            fRefreshRequest.End();
        }
    }
    

    void MessageManager::OnRefresh()
    {
        fRefreshCallback();
    }

    void MessageManager::OnTimer()
    {
        auto now = fStopWatch.GetElapsedTimeInteger(LLUtils::StopWatch::TimeUnit::Milliseconds);

        bool isFading = false;
        bool refreshNeeded = false;
        int32_t nextEvent = std::numeric_limits<int32_t>::max();

        std::vector<ListMessageData::iterator> elementsToRemove;

        for (auto it = fMessages.begin(); it != fMessages.end(); it++)
        {
            auto& messageData = *it;
            if (messageData.displayStage == DisplayStage::Hidden)
                break;

            switch (messageData.displayStage)
            {
            case DisplayStage::Hidden:
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Execution path shouldn't get here.");
                break;
            case DisplayStage::Visible:
            {
                const bool isManualRemove = (messageData.flags & MessageFlags::ManualRemove) == MessageFlags::ManualRemove;
                if (isManualRemove == false)
                {
                    auto dueDone = (messageData.timeStamp + messageData.ttl) - now;

                    if (dueDone <= 0)
                    {
                        messageData.displayStage = DisplayStage::Fading;
                        isFading = true;
                    }
                    else
                    {
                        nextEvent = std::min(nextEvent,  static_cast<int32_t>(dueDone));
                    }
                }
            }
                break;
            case DisplayStage::Fading:
            {
                double opacity = messageData.message->GetOpacity();

                constexpr double OpacityThreshold = 0.01;
                if (opacity > OpacityThreshold)
                {
                    isFading = true;
                    opacity *= 0.8;
                }
                else
                {
                    // Remove message from display.
                    
                    opacity = 1.0;
                    elementsToRemove.push_back(it);
                }

                messageData.message->SetOpacity(opacity);
                refreshNeeded |= messageData.message->IsDirty();
                
            }
                break;
            }
        }

        for (auto it : elementsToRemove)
            RemoveElement(it);

        UpdateMessagesPosition();

        fFadeTimer.SetInterval(isFading ? 5 : 0);

        if (nextEvent != std::numeric_limits<int32_t>::max())
        {
            fTimerHideUserMessage.SetInterval(nextEvent);
        }
        else
        {
            fTimerHideUserMessage.SetInterval(0);
        }

        if (refreshNeeded)
            fRefreshRequest.Queue();
    }


    void MessageManager::UpdateMessagesPosition()
    {
        bool needrefresh = false;
        int pos = fMarginTop;
        for (const auto& messageData : fMessages)
        {
            auto metrics = messageData.message->GetMetrics();
            messageData.message->SetPosition({ static_cast<double>(fMarginLeft), static_cast<double>(pos)});
            if (messageData.message->IsDirty())
                needrefresh |= true;

            pos += metrics.totalRows * metrics.rowHeight;
        }

        if (needrefresh)
            fRefreshRequest.Queue();
    }

    void MessageManager::RemoveGroup(GroupID groupID)
    {
        bool updateNeeded = false;
        auto it = fMessages.begin();
        while (it != fMessages.end())
        {
            if (it->groupID == groupID)
            {
                it = RemoveElement(it);
                updateNeeded = true;
            }
            else
            {
                it++;
            }
        }

        if (updateNeeded)
        {
            UpdateMessagesPosition();
            fRefreshRequest.Queue();
        }
    }

    void MessageManager::CreateMessageTemplate(MessageData& messageData)
    {
        messageData.messageUniqueID = fMessageIDProvider.Acquire();
        messageData.message = fLabelManager->GetOrCreateTextLabel("userMessage" + std::to_string(messageData.messageUniqueID));
        if (messageData.message == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to create user message.");

        messageData.message->SetBackgroundColor(LLUtils::Color(0));
        messageData.message->SetFontPath(LabelManager::sFontPath);
        messageData.message->SetFontSize(12);
        messageData.message->SetOutlineWidth(2);
        messageData.message->SetPosition(static_cast<LLUtils::PointF64>(LLUtils::PointI32(fMarginLeft,fMarginTop)));
        messageData.message->SetFilterType(OIV_Filter_type::FT_None);
        messageData.message->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
        messageData.message->SetScale({ 1.0,1.0 });
        messageData.message->SetOpacity(1.0);
        messageData.message->SetVisible(true);
        messageData.message->SetMaxWidth(fMaxMessageWidth);
    }
    

    MessageManager::ListMessageData::iterator MessageManager::FindVisible(GroupID groupID)
    {
        return std::find_if(fMessages.begin(), fMessages.end(), [&](const auto& messageData)
        {
            return (messageData.flags & MessageFlags::Persistent) == MessageFlags::None
                && messageData.groupID == groupID 
                && messageData.displayStage != DisplayStage::Hidden;
        });
        
    }

    MessageManager::ListMessageData::iterator MessageManager::RemoveElement(MessageManager::ListMessageData::iterator it)
    {
        fMessageIDProvider.Release(it->messageUniqueID);
        it->message->SetVisible(false);
        it->displayStage = DisplayStage::Hidden;
        return fMessages.erase(it);
    }

    void MessageManager::PushNextMessage(GroupID groupID, MessageFlags groupFlags, const std::wstring& message)
    {
        if (fMessages.size() + 1 > fMaxMessages)
        {
            ListMessageData::iterator lastElement = fMessages.end();
            std::advance(lastElement, -1);
            std::rotate(fMessages.begin(), lastElement, fMessages.end());
        }
        else
        {
            fMessages.push_front({});
        }


        auto& messageData =  *fMessages.begin();
        messageData.flags = groupFlags;
        messageData.groupID = groupID;
        
        UpdateMessage(message, messageData);
    }

    void MessageManager::UpdateMessage(const std::wstring& message, MessageData& messageData)
    {
        if (messageData.message == nullptr)
            CreateMessageTemplate(messageData);

        messageData.message->SetVisible(true);
        messageData.message->SetOpacity(1.0);
        messageData.message->SetText(message);
        messageData.displayStage = DisplayStage::Visible;
        messageData.ttl = std::max(fMinDelayRemoveMessage, static_cast<uint32_t>(message.length() * fDelayPerCharacter));
        messageData.timeStamp = fStopWatch.GetElapsedTimeInteger(LLUtils::StopWatch::TimeUnit::Milliseconds);

        if (fTimerHideUserMessage.GetInterval() == 0 || messageData.ttl < fTimerHideUserMessage.GetInterval())
            fTimerHideUserMessage.SetInterval(messageData.ttl);

        if (messageData.message->IsDirty())
            fRefreshRequest.Queue();
    }

    void MessageManager::SetUserMessage(uint32_t groupID, MessageFlags flags, const std::wstring& message)
    {
        const std::wstring wmsg = L"<textcolor=#ff8930>" + message;
        const bool isMoveable = (flags & MessageFlags::Moveable) == MessageFlags::Moveable;
        const bool isPersistent = (flags & MessageFlags::Persistent) == MessageFlags::Persistent;
        const bool isInterchangeable = (flags & MessageFlags::Interchangeable) == MessageFlags:: Interchangeable;

        fRefreshRequest.Begin();

        if (isPersistent == true)
        {
            PushNextMessage(groupID, flags, wmsg); // add message to queue, remove last messgae if queue full.
        }
        else if (isInterchangeable == true)
        {
            auto it = FindVisible(groupID);

            if (it == fMessages.end())
            {
                PushNextMessage(groupID, flags, wmsg); // add message to queue, remove last messgae if queue full.
            }
            else // found visible with same groupid
            {
                auto& messageData = *it;
                UpdateMessage(wmsg, messageData);
                if (isMoveable)
                    std::swap(*it, *fMessages.begin());
            }
        }
        UpdateMessagesPosition();

        fRefreshRequest.End();
    }
}