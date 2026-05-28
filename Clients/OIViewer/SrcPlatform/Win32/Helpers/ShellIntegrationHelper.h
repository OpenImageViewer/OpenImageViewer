#pragma once

#include <LLUtils/Rect.h>
#include <LLUtils/StringUtility.h>

#include <cstdint>
#include <string>

namespace OIV
{
    class ShellIntegrationHelper
    {
      public:
        template <typename RectType>
        static LLUtils::PointI32 TrayContextMenuPosition(const RectType& iconRect)
        {
            auto position = iconRect.GetCorner(LLUtils::Corner::TopRight);
            position.x += iconRect.GetWidth() / 2;
            position.y += iconRect.GetHeight() / 2;
            return {static_cast<int32_t>(position.x), static_cast<int32_t>(position.y)};
        }

        static std::string ViewCommandArgsFromTrayItem(const LLUtils::native_string_type& itemDisplayName)
        {
            return std::string("type=") +
                   LLUtils::StringUtility::ToLower(LLUtils::StringUtility::ConvertString<std::string>(
                       itemDisplayName));
        }
    };
}  // namespace OIV
