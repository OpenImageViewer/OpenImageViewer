#include "FileAssociationPrompt.h"
#include "WindowPlacement.h"
#include <LWSUI/Containers.hpp>
#include <cmath>

namespace OIV
{
    LWS::Result FileAssociationPrompt::Show(LWS::Window& owner, std::function<void(Choice)> respond)
    {
        if (!fWindow.IsCreated())
        {
            const auto result = fWindow.Create({.owner      = &owner,
                                                .modal      = true,
                                                .title      = LWSUI::NativeText("Open Image Viewer"),
                                                .clientSize = {520, 300},
                                                .styles = LWS::WindowStyle::Caption | LWS::WindowStyle::CloseButton});
            if (result != LWS::Result::Success)
                return result;
            auto events = fWindow.Listen(
                [this](const LWS::AnyEvent& event)
                {
                    if (std::holds_alternative<LWS::EventCloseRequested>(event))
                    {
                        Finish(Choice::AskNextTime);
                        return LWS::EventResponse::Handled;
                    }
                    return LWS::EventResponse::Unhandled;
                });
            if (!events)
            {
                std::ignore = fWindow.Destroy();
                return LWS::Result::Failure;
            }
            fEvents = std::move(*events);
            LWSUI::Theme theme;
            theme.font.size        = 13;
            theme.smallFont.size   = 11;
            theme.headingFont.size = 17;
            theme.rowHeight        = 32;
            fHost                  = std::make_unique<LWSUI::UIHost>(fWindow, theme);
            auto content           = std::make_unique<LWSUI::StackPanel>();
            content->spacing       = 16;
            auto& heading          = content->Emplace<LWSUI::Label>(
                "Make OIViewer the default for all supported image formats?");
            heading.fontRole  = LWSUI::Control::FontRole::Heading;
            heading.wrap      = true;
            auto& description = content->Emplace<LWSUI::Label>(
                "OIViewer is not the default for some supported image formats.\n\n"
                "Choose selectively to manage individual extensions in Settings > Filesystem > File associations. "
                "Changes take effect on Apply or Save.");
            description.wrap  = true;
            auto actions      = std::make_unique<LWSUI::StackPanel>();
            actions->spacing  = 10;
            auto primary      = std::make_unique<LWSUI::Grid>();
            primary->columns  = {-1, -1, -1};
            primary->spacing  = 8;
            const auto button = [this](LWSUI::Grid& row, const char* label, Choice choice) -> LWSUI::Button&
            {
                auto& control = row.Emplace<LWSUI::Button>(label);
                fConnections.Connect(control.OnClick, [this, choice] { Finish(choice); });
                return control;
            };
            button(*primary, "Associate all", Choice::AssociateAll);
            button(*primary, "Choose selectively...", Choice::Selective);
            fDefault = &button(*primary, "Ask next time", Choice::AskNextTime);
            // Symmetric padding centers the narrower primary row over the full-width opt-out row.
            auto primaryPanel     = std::make_unique<LWSUI::Panel>(std::move(primary));
            primaryPanel->padding = {24, 0, 24, 0};
            actions->Add(std::move(primaryPanel));
            auto& secondary   = actions->Emplace<LWSUI::Grid>();
            secondary.columns = {-1, -1};
            secondary.spacing = 8;
            button(secondary, "Don't ask for this copy", Choice::SuppressForCopy);
            button(secondary, "Don't ask for any copy", Choice::SuppressForUser);
            content->Add(std::move(actions));
            auto& footnote = content->Emplace<LWSUI::Label>(
                "Closing this window without choosing will ask again next time you start OIViewer.");
            footnote.fontRole = LWSUI::Control::FontRole::Small;
            footnote.SetForeground(theme.muted);
            footnote.wrap  = true;
            auto panel     = std::make_unique<LWSUI::Panel>(std::move(content));
            panel->padding = {20, 16, 20, 16};
            fHost->SetRoot(std::move(panel));
            constexpr int32_t width = 520;
            const auto measured     = fHost->Root()->Measure({width, 10000});
            const LWS::LogicalSize size{width, static_cast<int32_t>(std::ceil(measured.height))};
            std::ignore = fWindow.SetClientSizeLimits({.minimum = size});
            std::ignore = fWindow.RequestPlacement({.clientSize = size});
            fConnections.Connect(fHost->OnCommandInput,
                                 [this](const LWSUI::Input& input, bool& handled)
                                 {
                                     if (input.kind == LWSUI::InputKind::KeyDown && input.key == LWS::KeyCode::Escape)
                                     {
                                         handled = true;
                                         Finish(Choice::AskNextTime);
                                     }
                                 });
        }
        fRespond          = std::move(respond);
        std::ignore       = PlaceWindowNear(fWindow, owner, WindowPlacementHint::CenterInOwner);
        const auto result = fWindow.SetVisible(true);
        if (result == LWS::Result::Success)
        {
            std::ignore = fWindow.RequestActivation();
            fHost->Update();
            fHost->Focus(fDefault);
        }
        else
            fRespond = {};
        return result;
    }
    void FileAssociationPrompt::Hide()
    {
        fRespond = {};
        if (fWindow.IsCreated())
            std::ignore = fWindow.SetVisible(false);
    }
    void FileAssociationPrompt::Finish(Choice choice)
    {
        auto respond = std::move(fRespond);
        Hide();
        if (respond)
            respond(choice);
    }
}  // namespace OIV
