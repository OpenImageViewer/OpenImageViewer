#pragma once
#include "External/rapidxml/rapidxml.hpp"
#include <FileHelper.h>
#include <unordered_map>
#include <PlatformUtility.h>

namespace OIV
{
    typedef rapidxml::xml_node<> XmlNode;


    struct UserSettingsStructure
    {
        struct
        {
            struct
            {
                struct
                {
                    double x = 0.25;
                    double y = 0.25;
                } OuterMargins;
                struct
                {
                    double x = 0.1;
                    double y = 0.1;
                } InnerMargins;
                
                int SmallImageOffsetStyle = 0;
            } ZoomScrollState;
        } OIVSettings;
    };

    class  UserSettings
    {
    public:
        typedef rapidxml::xml_node<> XmlNode;
        typedef std::unordered_map<std::string, XmlNode*> MapXmlPathToNode;
        UserSettingsStructure settings;
        MapXmlPathToNode fMapXmlPathToNode;
        rapidxml::xml_document<> fDoc;
        XmlNode* fRootNode = nullptr;
        XmlNode* GetNode(std::string xmlPath);
        double GetDouble(std::string path);

        void SetDouble(std::string path, double num);

        void Load();
    };
}
