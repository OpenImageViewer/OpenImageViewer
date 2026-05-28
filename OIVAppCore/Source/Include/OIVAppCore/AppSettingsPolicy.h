#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/FileReloadPolicy.h>
#include <OIVShared/FileSorter.h>

#include <LLUtils/EnumClassBitwise.h>

#include <cstdint>
#include <string>

namespace OIV
{
    enum class DeletedFileRemovalMode
    {
        None              = 0 << 0,
        DeletedInternally = 1 << 0,
        DeletedExternally = 1 << 1,
        Always            = 3
    };

    LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(DeletedFileRemovalMode);

    template <typename T>
    struct ParsedSetting
    {
        bool valid = false;
        T value{};
    };

    class AppSettingsPolicy
    {
      public:

        enum class ActionType
        {
            None,
            MaxZoom,
            ImageMarginX,
            ImageMarginY,
            MinImageSize,
            SlideshowInterval,
            QuickBrowseDelay,
            AutoScrollDeadZoneRadius,
            AutoScrollSpeedFactorIn,
            AutoScrollSpeedFactorOut,
            AutoScrollSpeedFactorRange,
            AutoScrollMaxSpeed,
            DeletedFileRemovalMode,
            FileReloadMode,
            ReloadSettingsFileIfChanged,
            DefaultSortMode,
            SortDirection,
            BackgroundColor,
            BiggestSubImageOnLoad
        };

        struct Action
        {
            ActionType type                               = ActionType::None;
            double floatValue                             = 0.0;
            int64_t integralValue                         = 0;
            bool boolValue                                = false;
            DeletedFileRemovalMode deletedFileRemovalMode = DeletedFileRemovalMode::None;
            FileReloadMode fileReloadMode                 = FileReloadMode::None;
            FileSorter::SortType sortType                 = FileSorter::SortType::Name;
            FileSorter::SortDirection sortDirection       = FileSorter::SortDirection::Ascending;
            uint32_t backgroundColorIndex                 = 0;
            LLUtils::native_string_type textValue;
        };

        static int64_t ParseIntegral(const LLUtils::native_string_type& value);
        static double ParseFloat(const LLUtils::native_string_type& value);
        static bool ParseBool(const LLUtils::native_string_type& value);
        static Action ParseAction(const LLUtils::native_string_type& key, const LLUtils::native_string_type& value);

        static ParsedSetting<DeletedFileRemovalMode> ParseDeletedFileRemovalMode(
            const LLUtils::native_string_type& value);
        static ParsedSetting<FileReloadMode> ParseFileReloadMode(const LLUtils::native_string_type& value);
        static ParsedSetting<FileSorter::SortType> ParseSortType(const LLUtils::native_string_type& value);
        static FileSorter::SortDirection ParseSortDirection(const LLUtils::native_string_type& value);
        static ParsedSetting<FileSorter::SortType> ParseSortDirectionTarget(const LLUtils::native_string_type& key);
        static ParsedSetting<uint32_t> ParseBackgroundColorIndex(const LLUtils::native_string_type& key);
    };
}  // namespace OIV
