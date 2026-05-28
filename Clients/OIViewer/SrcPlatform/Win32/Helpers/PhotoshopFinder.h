#pragma once
#include <algorithm>
#include <vector>
#include <filesystem>
#include <WinReg.hpp>


namespace OIV
{
    class PhotoshopFinder
    {
    public:

        struct greater
        {
            bool operator()(const LLUtils::native_string_type &a, LLUtils::native_string_type const &b) const
            {
                return std::stod(a) > std::stod(b);
            }
        };

        static LLUtils::native_string_type FindPhotoshop()
        {
            using namespace std;
            using namespace winreg;
            try
            {
                RegKey key{ HKEY_LOCAL_MACHINE, LLUTILS_TEXT("SOFTWARE\\Adobe\\Photoshop"),KEY_READ };

                vector<wstring> subKeyNames = key.EnumSubKeys();
                std::vector<LLUtils::native_string_type>  versions;
                for (const auto& s : subKeyNames)
                    versions.push_back(s);

                std::sort(versions.begin(), versions.end(), std::greater<decltype(versions)::value_type>());

                for (auto& version : versions)
                {
                    try
                    {
                        RegKey versionkey{ HKEY_LOCAL_MACHINE, LLUTILS_TEXT("SOFTWARE\\Adobe\\Photoshop\\") + version,KEY_READ };

                        const LLUtils::native_string_type appPath = LLUtils::native_string_type(LLUTILS_TEXT("ApplicationPath"));
                        LLUtils::native_string_type fileName = versionkey.GetStringValue(appPath) + LLUTILS_TEXT("\\Photoshop.exe");
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
            return LLUtils::native_string_type();
        }
    };
}
