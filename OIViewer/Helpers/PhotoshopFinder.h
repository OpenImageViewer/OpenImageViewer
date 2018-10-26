#pragma once
#include <exception>
#include <iostream>
#include <algorithm>
#include <vector>
#include <filesystem>
#include "StringUtility.h"
#include "../External/WinReg.hpp"


namespace OIV
{
    class PhotoShopFinder
    {
    public:

        struct greater
        {
            bool operator()(const std::wstring &a, std::wstring const &b) const
            {
                return std::atof(LLUtils::StringUtility::ToAString(a).c_str()) > std::atof(LLUtils::StringUtility::ToAString(b).c_str());
            }
        };

        static std::wstring FindPhotoShop()
        {
            using namespace std;
            using namespace winreg;
            try
            {
                RegKey key{ HKEY_LOCAL_MACHINE, L"SOFTWARE\\Adobe\\Photoshop",KEY_READ };

                vector<wstring> subKeyNames = key.EnumSubKeys();
                wcout << L"Subkeys:\n";
                std::vector<std::wstring>  versions;
                for (const auto& s : subKeyNames)
                    versions.push_back(s);

                std::sort(versions.begin(), versions.end(), std::greater<decltype(versions)::value_type>());

                for (auto& version : versions)
                {
                    try
                    {
                        RegKey versionkey{ HKEY_LOCAL_MACHINE, L"SOFTWARE\\Adobe\\Photoshop\\" + version,KEY_READ };

                        const std::wstring appPath = std::wstring(L"ApplicationPath");
                        std::wstring fileName = versionkey.GetStringValue(appPath) + L"\\Photoshop.exe";
                        if (std::filesystem::exists(fileName))
                            return fileName;
                    }
                    catch (...) {

                    }
                }
            }
            catch (...)
            {

            }
            return std::wstring();
        }
    };
}