#pragma once
#include <string>
#include <fstream>
#include <sstream>

namespace OIV
{
    class File
    {
    public:
        static std::string ReadAllText(std::wstring filePath)
        {
            using namespace std;
            ifstream t(filePath);
            stringstream buffer;
            buffer << t.rdbuf();
            return buffer.str();
        }

        //static void ReadAllBytes(std::wstring filePath,size_t& size, void*& buffer)
        //{
        //    using namespace std;
        //    ifstream t(filePath);
        //    memorys
        //    stringstream buffer;
        //    buffer << t.rdbuf();
        //    return buffer.str();
        //}

    };
}
