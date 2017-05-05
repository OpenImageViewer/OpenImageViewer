
#pragma once
#include <filesystem>

namespace OIV
{
    class FileSystemHelper
    {
    public:
        static std::wstring ResolveFullPath(std::wstring fileName)
        {
            using namespace std::experimental::filesystem;
            path p(fileName);
            if (p.empty() == false && p.is_absolute() == false)
            {
                p = current_path() / fileName;
                if (exists(p) == false)
                    p.clear();

            }

            return p.wstring();
        }
    };
}
