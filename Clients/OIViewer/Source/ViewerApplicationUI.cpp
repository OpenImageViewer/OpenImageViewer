#include "ViewerApplication.h"
#include <LLUtils/Exception.h>
#include <LLUtils/StringUtility.h>
#include <OIVAppCore/ShellIntegrationHelper.h>

namespace OIV
{
    void ViewerApplication::NetSettingsCallback_(ItemChangedArgs* args)
    {
        reinterpret_cast<ViewerApplication*>(args->userData)->NetSettingsCallback(args);
    }

    void ViewerApplication::NetSettingsCallback(ItemChangedArgs* args)
    {
        fViewer.OnSettingChange(LLUtils::StringUtility::ToNativeString(std::wstring(args->key)),
                                LLUtils::StringUtility::ToNativeString(std::wstring(args->val)));
    }

    void ViewerApplication::OnNotificationIcon(LWS::NotificationIconGroup::NotificationIconEventArgs args)
    {
        if (fViewer.fIsShuttingDown)
            return;
        switch (args.action)
        {
            case LWS::NotificationIconGroup::NotificationIconAction::Select:
                if (!fViewer.fWindow.GetWindow().IsVisible() ||
                    fViewer.fWindow.GetWindow().GetShowState() == LWS::WindowShowState::Minimized)
                {
                    std::ignore = fViewer.fWindow.GetWindow().SetVisible(true);
                    std::ignore = fViewer.fWindow.GetWindow().RequestShowState(LWS::WindowShowState::Restored);
                    std::ignore = fViewer.fWindow.GetWindow().RequestActivation();
                }
                else
                {
                    std::ignore = fViewer.fWindow.GetWindow().SetVisible(false);
                }
                break;
            case LWS::NotificationIconGroup::NotificationIconAction::ContextMenu:
            {
                auto rect       = GetNotificationIconRect(fNotificationIconID);
                auto bottomLeft = ShellIntegrationHelper::TrayContextMenuPosition(rect);

                std::ignore     = fViewer.fWindow.GetWindow().RequestActivation();
                auto chosenItem = fNotificationContextMenu->Show(bottomLeft.x, bottomLeft.y, AlignmentHorizontal::Right,
                                                                 AlignmentVertical::Bottom);
                if (chosenItem != nullptr)
                {
                    Viewer::CommandRequestIntenal request;
                    request.commandName = "cmd_view_state";
                    request.args = ShellIntegrationHelper::ViewCommandArgsFromTrayItem(chosenItem->itemDisplayName);
                    fViewer.ExecuteCommandInternal(request);
                }
            }
            break;
            case LWS::NotificationIconGroup::NotificationIconAction::None:
                LL_EXCEPTION_UNEXPECTED_VALUE;
                break;
        }
    }
}  // namespace OIV
