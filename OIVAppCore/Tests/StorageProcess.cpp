#include <OIVAppCore/UserStorage.h>
#include <LLUtils/Exception.h>
#include <LLUtils/StringUtility.h>
#include <fstream>
#include <iostream>
#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    LLUtils::Exception::RegisterMainThread();
    try
    {
        if (argc < 3 || argc > 4)
            return 2;
        const std::filesystem::path root(argv[1]);
        const OIV::StorageSession session({root / "Config", root / "State", root / "Cache"},
                                          LLUtils::StringUtility::ToAString(argv[2]));
        if (argc == 4)
        {
            std::ofstream(argv[3]) << "ready";
            std::cin.get();
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what();
        return 1;
    }
}
