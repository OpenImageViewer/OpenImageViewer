#pragma once
#include "BuildConfig.h"

#if OIV_VIEWER_BUILD_ESJ == 1
    #include <json_writer.h>
    #include <json_reader.h>
#else

// define empty stubs 
namespace JSON
{
    class Adapter{};
    class Class
    {
    public:
        Class(Adapter, char*) {}
        
    };
}

#define JSON_E(X,Y)
    
#endif

#include <PlatformUtility.h>



namespace OIV
{
    ///*
    namespace Serialization
    {
        struct Point
        {
            double x;
            double y;

            void serialize(JSON::Adapter& adapter)
            {
                // this pattern is required 
                JSON::Class root(adapter, "Point");
                JSON_E(adapter, x);
                JSON_E(adapter, y);
            }
        };

        struct ZoomScrollState
        {
            Point OuterMargins = { 0.25,0.25 };
            Point InnnerMargins = { 0.1,0.1 };
            int smallImageOffsetStyle = 0;

            void serialize(JSON::Adapter& adapter)
            {
                // this pattern is required 
                JSON::Class root(adapter, "ZoomScrollState");
                JSON_E(adapter, smallImageOffsetStyle);
                JSON_E(adapter, OuterMargins);
                JSON_E(adapter, InnnerMargins);
            }
        };




        
        struct UserSettingsData
        {
            ZoomScrollState zoomScrollState;
            void serialize(JSON::Adapter& adapter)
            {
                // this pattern is required 
                JSON::Class root(adapter, "OIVViewerettings");
                JSON_E(adapter, zoomScrollState);
            }
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
