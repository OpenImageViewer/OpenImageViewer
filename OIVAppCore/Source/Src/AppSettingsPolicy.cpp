#include <OIVAppCore/AppSettingsPolicy.h>

namespace OIV
{
    int64_t AppSettingsPolicy::ParseIntegral(const std::wstring& value)
    {
        return std::stoll(value);
    }

    double AppSettingsPolicy::ParseFloat(const std::wstring& value)
    {
        return std::stod(value);
    }

    bool AppSettingsPolicy::ParseBool(const std::wstring& value)
    {
        return value == L"true";
    }

    AppSettingsPolicy::Action AppSettingsPolicy::ParseAction(const std::wstring& key, const std::wstring& value)
    {
        if (key == L"viewsettings/maxzoom")
            return {ActionType::MaxZoom, ParseFloat(value)};
        if (key == L"viewsettings/imagemargins/x")
            return {ActionType::ImageMarginX, ParseFloat(value)};
        if (key == L"viewsettings/imagemargins/y")
            return {ActionType::ImageMarginY, ParseFloat(value)};
        if (key == L"viewsettings/minimagesize")
            return {ActionType::MinImageSize, ParseFloat(value)};
        if (key == L"viewsettings/slideshowinterval")
            return {ActionType::SlideshowInterval, 0.0, ParseIntegral(value)};
        if (key == L"viewsettings/quickbrowsedelay")
            return {ActionType::QuickBrowseDelay, 0.0, ParseIntegral(value)};
        if (key == L"autoscroll/deadzoneradius")
            return {ActionType::AutoScrollDeadZoneRadius, 0.0, ParseIntegral(value)};
        if (key == L"autoscroll/speedfactorin")
            return {ActionType::AutoScrollSpeedFactorIn, ParseFloat(value)};
        if (key == L"autoscroll/speedfactorout")
            return {ActionType::AutoScrollSpeedFactorOut, ParseFloat(value)};
        if (key == L"autoscroll/speedfactorrange")
            return {ActionType::AutoScrollSpeedFactorRange, 0.0, ParseIntegral(value)};
        if (key == L"autoscroll/maxspeed")
            return {ActionType::AutoScrollMaxSpeed, 0.0, ParseIntegral(value)};

        if (key == L"filesystem/deletedfileremovalmode")
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

        if (key == L"filesystem/modifiedfilereloadmode")
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

        if (key == L"system/reloadsettingsfileifchanged")
            return {ActionType::ReloadSettingsFileIfChanged, 0.0, 0, ParseBool(value)};

        if (key == L"files/defaultsortmode")
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
            action.sortType = sortDirectionTarget.value;
            action.sortDirection = ParseSortDirection(value);
            return action;
        }

        if (const ParsedSetting<uint32_t> backgroundColorIndex = ParseBackgroundColorIndex(key);
            backgroundColorIndex.valid)
        {
            Action action{ActionType::BackgroundColor};
            action.backgroundColorIndex = backgroundColorIndex.value;
            action.textValue = value;
            return action;
        }

        if (key == L"imagesettings/biggestsubimage")
            return {ActionType::BiggestSubImageOnLoad, 0.0, 0, ParseBool(value)};

        return {};
    }

    ParsedSetting<DeletedFileRemovalMode> AppSettingsPolicy::ParseDeletedFileRemovalMode(const std::wstring& value)
    {
        if (value == L"always")
            return {true, DeletedFileRemovalMode::Always};
        if (value == L"externally")
            return {true, DeletedFileRemovalMode::DeletedExternally};
        if (value == L"internally")
            return {true, DeletedFileRemovalMode::DeletedInternally};
        if (value == L"none")
            return {true, DeletedFileRemovalMode::None};

        return {};
    }

    ParsedSetting<FileReloadMode> AppSettingsPolicy::ParseFileReloadMode(const std::wstring& value)
    {
        if (value == L"none")
            return {true, FileReloadMode::None};
        if (value == L"confirmation")
            return {true, FileReloadMode::Confirmation};
        if (value == L"autoforeground")
            return {true, FileReloadMode::AutoForeground};
        if (value == L"autobackground")
            return {true, FileReloadMode::AutoBackground};

        return {};
    }

    ParsedSetting<FileSorter::SortType> AppSettingsPolicy::ParseSortType(const std::wstring& value)
    {
        if (value == L"name")
            return {true, FileSorter::SortType::Name};
        if (value == L"date")
            return {true, FileSorter::SortType::Date};
        if (value == L"extension")
            return {true, FileSorter::SortType::Extension};

        return {};
    }

    FileSorter::SortDirection AppSettingsPolicy::ParseSortDirection(const std::wstring& value)
    {
        return value == L"ascending" ? FileSorter::SortDirection::Ascending : FileSorter::SortDirection::Descending;
    }

    ParsedSetting<FileSorter::SortType> AppSettingsPolicy::ParseSortDirectionTarget(const std::wstring& key)
    {
        if (key == L"files/sortbynamedirection")
            return {true, FileSorter::SortType::Name};
        if (key == L"files/sortbydatedirection")
            return {true, FileSorter::SortType::Date};
        if (key == L"files/sortbyextensiondirection")
            return {true, FileSorter::SortType::Extension};

        return {};
    }

    ParsedSetting<uint32_t> AppSettingsPolicy::ParseBackgroundColorIndex(const std::wstring& key)
    {
        if (key == L"displaysettings/backgroundcolor1")
            return {true, 0};
        if (key == L"displaysettings/backgroundcolor2")
            return {true, 1};

        return {};
    }
}
