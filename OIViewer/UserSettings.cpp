#include "UserSettings.h"
namespace OIV
{
    XmlNode* UserSettings::GetNode(std::string xmlPath)
    {
        auto it = fMapXmlPathToNode.find(xmlPath);
        if (it == fMapXmlPathToNode.end())
        {
            int idx = xmlPath.find_last_of(".");
            if (idx >= 0)
            {
                std::string parentNodeName = xmlPath.substr(0, idx);
                std::string childNodeName = xmlPath.substr(idx + 1, xmlPath.length() - idx);

                rapidxml::xml_node<>* parent = GetNode(parentNodeName);
                if (parent != nullptr)
                {
                    XmlNode* node = parent->first_node(childNodeName.c_str());
                    if (node)
                    {
                        fMapXmlPathToNode.insert(std::make_pair(xmlPath.c_str(), node));
                        return node;
                    }
                    else
                    {
                        return nullptr;
                    }
                }
            }
            else
            {
                std::string parentNodeName = xmlPath.substr(0, idx);
                rapidxml::xml_node<>* parent = fDoc.first_node(parentNodeName.c_str());
                if (parent != nullptr)
                {
                    fMapXmlPathToNode.insert(std::make_pair(parentNodeName, parent));
                    return parent;
                }
                else
                {
                    return nullptr;
                }
            }
        }
        else
            return it->second;
    }

    double UserSettings::GetDouble(std::string path)
    {
        XmlNode* node = GetNode(path.c_str());
        return std::stod(node->value());

    }

    void UserSettings::SetDouble(std::string path, double num)
    {
        XmlNode* node = GetNode(path.c_str());
        node->value(node->document()->allocate_string(std::to_string(num).c_str()));
    }

    void UserSettings::Load()
    {   
        using namespace std;
        using namespace rapidxml;
        using path = experimental::filesystem::path;
        path settingsPath = path(LLUtils::PlatformUtility::GetAppDataFolder()) / L"settings.xml";

        string str = LLUtils::File::ReadAllText(settingsPath);
        if (str.empty() == false)
        {
            char* text = new char[str.length() + 1];
            strcpy_s(text, sizeof(char) * (str.length() + 1), str.c_str());
            fDoc.parse<0>(text);
            fRootNode = fDoc.first_node("OIVSettings");

            settings.OIVSettings.ZoomScrollState.InnerMargins.x = GetDouble("OIVSettings.ZoomScrollState.InnerMargins.x");
            settings.OIVSettings.ZoomScrollState.InnerMargins.y = GetDouble("OIVSettings.ZoomScrollState.InnerMargins.y");
            settings.OIVSettings.ZoomScrollState.OuterMargins.x = GetDouble("OIVSettings.ZoomScrollState.OuterMargins.x");
            settings.OIVSettings.ZoomScrollState.OuterMargins.y = GetDouble("OIVSettings.ZoomScrollState.OuterMargins.y");
            settings.OIVSettings.ZoomScrollState.SmallImageOffsetStyle = (int)GetDouble("OIVSettings.ZoomScrollState.SmallImageOffsetStyle");
        }
    }
}
