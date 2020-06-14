#pragma once
#include <LLUtils/PlatformUtility.h>

namespace OIV
{
    ///*
    namespace Serialization
    {
        struct Point
        {
            double x;
            double y;
        };

        struct ZoomScrollState
        {
            Point Margins = { 0.75,0.75 };
            int smallImageOffsetStyle = 0;
        };
        
        struct UserSettingsData
        {
            ZoomScrollState zoomScrollState;
        };

    }


    class  UserSettings
    {
    public:
        Serialization::UserSettingsData fSettingsData;

        Serialization::UserSettingsData& getUserSettings();

        std::wstring GetSettingsPath();
        void Save();
        void Load();
    };
}
