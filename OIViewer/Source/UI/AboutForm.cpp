#include "AboutForm.h"
#include "WindowPlacement.h"
#include <Version.h>
#include <LWSUI/Containers.hpp>
#include <LWSUI/Primitives.hpp>
#include <LLUtils/StringUtility.h>
#include <cmath>

namespace OIV
{
    LWS::Result AboutForm::Show(LWS::Window& owner, LWS::BitmapSharedPtr icon, std::string buildTime)
    {
        if (!fWindow.IsCreated())
        {
            const auto result = fWindow.Create({.owner      = &owner,
                                                .modal      = true,
                                                .title      = LLUtils::StringUtility::ToNativeString("About OIViewer"),
                                                .clientSize = {440, 360},
                                                .styles = LWS::WindowStyle::Caption | LWS::WindowStyle::CloseButton});
            if (result != LWS::Result::Success)
                return result;
            auto events = fWindow.Listen(
                [this](const LWS::AnyEvent& event)
                {
                    if (std::holds_alternative<LWS::EventCloseRequested>(event))
                    {
                        std::ignore = fWindow.SetVisible(false);
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
            if (icon)
                if (auto native = LWS::WindowIcon::FromBitmap(icon->GetBuffer()))
                    std::ignore = fWindow.SetWindowIcon(std::move(*native));
            LWSUI::Theme theme;
            theme.font.size = theme.smallFont.size = 13;
            theme.headingFont.size                 = 17;
            theme.rowHeight                        = 24;
            theme.textPadding                      = 3;
            fHost                                  = std::make_unique<LWSUI::UIHost>(fWindow, theme);
            auto content                           = std::make_unique<LWSUI::StackPanel>();
            content->spacing                       = 8;
            auto header                            = std::make_unique<LWSUI::Grid>();
            header->columns                        = {32, -1};
            header->spacing                        = 10;
            auto image                             = std::make_unique<LWSUI::Image>();
            image->size                            = 32;
            image->SetBitmap(std::move(icon));
            header->Add(std::move(image));
            auto title               = std::make_unique<LWSUI::Label>("OpenImageViewer");
            title->fontRole          = LWSUI::Control::FontRole::Heading;
            title->verticalAlignment = LWSUI::Alignment::Center;
            header->Add(std::move(title));
            content->Add(std::move(header));
            auto fields     = std::make_unique<LWSUI::Grid>();
            fields->columns = {92, -1};
            fields->spacing = 2;
            auto field      = [&](std::string label, std::string value)
            {
                auto name               = std::make_unique<LWSUI::Label>(std::move(label));
                name->fontRole          = LWSUI::Control::FontRole::Small;
                name->verticalAlignment = LWSUI::Alignment::Center;
                fields->Add(std::move(name));
                auto text = std::make_unique<LWSUI::TextBox>(std::move(value));
                text->SetReadOnly(true);
                text->SetFocusOutline(true);
                fields->Add(std::move(text));
            };
            auto version = FormatFullVersion(CurrentVersion);
#ifdef OIV_RELEASE_SUFFIX
            version += LLUtils::StringUtility::ConvertString<std::string>(std::basic_string_view{OIV_RELEASE_SUFFIX});
#endif
            field("Version", std::move(version));
            field("Revision", OIV_GIT_SHORT_HASH);
            field("Architecture", std::to_string(sizeof(void*) * 8) + " bit");
#ifdef _DEBUG
            field("Configuration", "Debug");
#else
            field("Configuration", "Release");
#endif
#if OIV_OFFICIAL_BUILD
            field("Distribution", "Official");
#else
            field("Distribution", "Unofficial");
#endif
            field("Built", std::move(buildTime));
            content->Add(std::move(fields));
            auto close = std::make_unique<LWSUI::Button>("Close");
            fConnections.Connect(close->OnClick, [this] { std::ignore = fWindow.SetVisible(false); });
            content->Add(std::move(close));
            auto panel     = std::make_unique<LWSUI::Panel>(std::move(content));
            panel->padding = {16, 12, 16, 12};
            fHost->SetRoot(std::move(panel));
            // Measure the complete content instead of budgeting a fixed height for the fields.
            // Keep placement from shrinking the dialog below the size its controls require.
            constexpr int32_t width = 416;
            const auto measured     = fHost->Root()->Measure({width, 10000});
            const LWS::LogicalSize size{width, static_cast<int32_t>(std::ceil(measured.height))};
            std::ignore = fWindow.SetClientSizeLimits({.minimum = size});
            std::ignore = fWindow.RequestPlacement({.clientSize = size});
            fConnections.Connect(fHost->OnCommandInput,
                                 [this](const LWSUI::Input& input, bool& handled)
                                 {
                                     if (input.kind == LWSUI::InputKind::KeyDown && input.key == LWS::KeyCode::Escape)
                                     {
                                         handled     = true;
                                         std::ignore = fWindow.SetVisible(false);
                                     }
                                 });
        }
        std::ignore       = PlaceWindowNear(fWindow, owner, WindowPlacementHint::CenterInOwner);
        const auto result = fWindow.SetVisible(true);
        if (result == LWS::Result::Success)
            std::ignore = fWindow.RequestActivation();
        return result;
    }
}  // namespace OIV
