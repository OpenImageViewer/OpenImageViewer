#include <LLUtils/StringDefs.h>
#include <OIVAppCore/AppSettingsPolicy.h>

#include <cmath>

namespace OIV
{
    int64_t AppSettingsPolicy::ParseIntegral(const LLUtils::native_string_type& value)
    {
        return std::stoll(value);
    }

    double AppSettingsPolicy::ParseFloat(const LLUtils::native_string_type& value)
    {
        return std::stod(value);
    }

    bool AppSettingsPolicy::ParseBool(const LLUtils::native_string_type& value)
    {
        return value == LLUTILS_TEXT("true");
    }

    AppSettingsPolicy::Action AppSettingsPolicy::ParseAction(const LLUtils::native_string_type& key,
                                                             const LLUtils::native_string_type& value)
    {
        if (key == LLUTILS_TEXT("viewsettings/maxzoom"))
            return {ActionType::MaxZoom, ParseFloat(value)};
        if (key == LLUTILS_TEXT("viewsettings/imagemargins/x"))
            return {ActionType::ImageMarginX, ParseFloat(value)};
        if (key == LLUTILS_TEXT("viewsettings/imagemargins/y"))
            return {ActionType::ImageMarginY, ParseFloat(value)};
        if (key == LLUTILS_TEXT("viewsettings/minimagesize"))
            return {ActionType::MinImageSize, ParseFloat(value)};
        if (key == LLUTILS_TEXT("viewsettings/slideshowinterval"))
            return {ActionType::SlideshowInterval, 0.0, ParseIntegral(value)};
        if (key == LLUTILS_TEXT("viewsettings/quickbrowsedelay"))
            return {ActionType::QuickBrowseDelay, 0.0, ParseIntegral(value)};
        if (key == LLUTILS_TEXT("animation/minframeintervalms") ||
            key == LLUTILS_TEXT("animation/slowframeintervalms") || key == LLUTILS_TEXT("animation/slowspeedpercent"))
        {
            // Settings JSON can represent milliseconds as either 3 or 3.0. Reject fractional,
            // malformed and nonfinite values before converting them to the typed action.
            Action action;
            try
            {
                size_t end          = 0;
                const double parsed = std::stod(value, &end);
                if (end == value.size() && std::isfinite(parsed) && parsed > 0.0)
                {
                    if (key == LLUTILS_TEXT("animation/slowspeedpercent"))
                    {
                        if (parsed <= 100.0)
                            action = {ActionType::AnimationSlowSpeedPercent, parsed};
                    }
                    else if (parsed <= SequencerPolicy::MaxTimerIntervalMs && std::trunc(parsed) == parsed)
                    {
                        action = {key == LLUTILS_TEXT("animation/minframeintervalms")
                                      ? ActionType::AnimationMinFrameInterval
                                      : ActionType::AnimationSlowFrameInterval,
                                  0.0, static_cast<int64_t>(parsed)};
                    }
                }
            }
            catch (const std::invalid_argument&)
            {
            }
            catch (const std::out_of_range&)
            {
            }
            return action;
        }
        if (key == LLUTILS_TEXT("autoscroll/deadzoneradius"))
            return {ActionType::AutoScrollDeadZoneRadius, 0.0, ParseIntegral(value)};
        if (key == LLUTILS_TEXT("autoscroll/speedfactorin"))
            return {ActionType::AutoScrollSpeedFactorIn, ParseFloat(value)};
        if (key == LLUTILS_TEXT("autoscroll/speedfactorout"))
            return {ActionType::AutoScrollSpeedFactorOut, ParseFloat(value)};
        if (key == LLUTILS_TEXT("autoscroll/speedfactorrange"))
            return {ActionType::AutoScrollSpeedFactorRange, 0.0, ParseIntegral(value)};
        if (key == LLUTILS_TEXT("autoscroll/maxspeed"))
            return {ActionType::AutoScrollMaxSpeed, 0.0, ParseIntegral(value)};

        if (key == LLUTILS_TEXT("filesystem/deletedfileremovalmode"))
        {
            const ParsedSetting<DeletedFileRemovalMode> mode = ParseDeletedFileRemovalMode(value);
            if (mode.valid)
            {
                Action action{ActionType::DeletedFileRemovalMode};
                action.deletedFileRemovalMode = mode.value;
                return action;
            }
            return {};
        }

        if (key == LLUTILS_TEXT("filesystem/modifiedfilereloadmode"))
        {
            const ParsedSetting<FileReloadMode> mode = ParseFileReloadMode(value);
            if (mode.valid)
            {
                Action action{ActionType::FileReloadMode};
                action.fileReloadMode = mode.value;
                return action;
            }
            return {};
        }

        if (key == LLUTILS_TEXT("system/reloadsettingsfileifchanged"))
            return {ActionType::ReloadSettingsFileIfChanged, 0.0, 0, ParseBool(value)};

        if (key == LLUTILS_TEXT("files/defaultsortmode"))
        {
            const ParsedSetting<FileSorter::SortType> sortType = ParseSortType(value);
            if (sortType.valid)
            {
                Action action{ActionType::DefaultSortMode};
                action.sortType = sortType.value;
                return action;
            }
            return {};
        }

        if (const ParsedSetting<FileSorter::SortType> sortDirectionTarget = ParseSortDirectionTarget(key);
            sortDirectionTarget.valid)
        {
            Action action{ActionType::SortDirection};
            action.sortType      = sortDirectionTarget.value;
            action.sortDirection = ParseSortDirection(value);
            return action;
        }

        if (const ParsedSetting<uint32_t> backgroundColorIndex = ParseBackgroundColorIndex(key);
            backgroundColorIndex.valid)
        {
            Action action{ActionType::BackgroundColor};
            action.backgroundColorIndex = backgroundColorIndex.value;
            action.textValue            = value;
            return action;
        }

        if (key == LLUTILS_TEXT("imagesettings/biggestsubimage"))
            return {ActionType::BiggestSubImageOnLoad, 0.0, 0, ParseBool(value)};

        return {};
    }

    bool AppSettingsPolicy::StageAnimationSetting(const Action& action, SequencerPolicy::Settings& settings)
    {
        auto candidate = settings;
        bool handled   = true;
        switch (action.type)
        {
            case ActionType::AnimationMinFrameInterval:
                candidate.minFrameIntervalMs = static_cast<uint32_t>(action.integralValue);
                break;
            case ActionType::AnimationSlowFrameInterval:
                candidate.slowFrameIntervalMs = static_cast<uint32_t>(action.integralValue);
                break;
            case ActionType::AnimationSlowSpeedPercent:
                candidate.slowSpeedPercent = action.floatValue;
                break;
            default:
                handled = false;
                break;
        }
        if (handled && candidate.IsValid())
            settings = candidate;
        return handled;
    }

    ParsedSetting<DeletedFileRemovalMode> AppSettingsPolicy::ParseDeletedFileRemovalMode(
        const LLUtils::native_string_type& value)
    {
        if (value == LLUTILS_TEXT("always"))
            return {true, DeletedFileRemovalMode::Always};
        if (value == LLUTILS_TEXT("externally"))
            return {true, DeletedFileRemovalMode::DeletedExternally};
        if (value == LLUTILS_TEXT("internally"))
            return {true, DeletedFileRemovalMode::DeletedInternally};
        if (value == LLUTILS_TEXT("none"))
            return {true, DeletedFileRemovalMode::None};

        return {};
    }

    ParsedSetting<FileReloadMode> AppSettingsPolicy::ParseFileReloadMode(const LLUtils::native_string_type& value)
    {
        if (value == LLUTILS_TEXT("none"))
            return {true, FileReloadMode::None};
        if (value == LLUTILS_TEXT("confirmation"))
            return {true, FileReloadMode::Confirmation};
        if (value == LLUTILS_TEXT("autoforeground"))
            return {true, FileReloadMode::AutoForeground};
        if (value == LLUTILS_TEXT("autobackground"))
            return {true, FileReloadMode::AutoBackground};

        return {};
    }

    ParsedSetting<FileSorter::SortType> AppSettingsPolicy::ParseSortType(const LLUtils::native_string_type& value)
    {
        if (value == LLUTILS_TEXT("name"))
            return {true, FileSorter::SortType::Name};
        if (value == LLUTILS_TEXT("date"))
            return {true, FileSorter::SortType::Date};
        if (value == LLUTILS_TEXT("extension"))
            return {true, FileSorter::SortType::Extension};

        return {};
    }

    FileSorter::SortDirection AppSettingsPolicy::ParseSortDirection(const LLUtils::native_string_type& value)
    {
        return value == LLUTILS_TEXT("ascending") ? FileSorter::SortDirection::Ascending
                                                  : FileSorter::SortDirection::Descending;
    }

    ParsedSetting<FileSorter::SortType> AppSettingsPolicy::ParseSortDirectionTarget(
        const LLUtils::native_string_type& key)
    {
        if (key == LLUTILS_TEXT("files/sortbynamedirection"))
            return {true, FileSorter::SortType::Name};
        if (key == LLUTILS_TEXT("files/sortbydatedirection"))
            return {true, FileSorter::SortType::Date};
        if (key == LLUTILS_TEXT("files/sortbyextensiondirection"))
            return {true, FileSorter::SortType::Extension};

        return {};
    }

    ParsedSetting<uint32_t> AppSettingsPolicy::ParseBackgroundColorIndex(const LLUtils::native_string_type& key)
    {
        if (key == LLUTILS_TEXT("displaysettings/backgroundcolor1"))
            return {true, 0};
        if (key == LLUTILS_TEXT("displaysettings/backgroundcolor2"))
            return {true, 1};

        return {};
    }
}  // namespace OIV
