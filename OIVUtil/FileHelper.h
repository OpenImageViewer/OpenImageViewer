#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

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

        static void ReadAllBytes(std::wstring filePath,size_t& size, uint8_t*& buffer)
        {
            using namespace std;
            using namespace std::experimental;
            size_t fileSize = filesystem::file_size(filePath);
            
            uint8_t* buf = new uint8_t[fileSize];
            ifstream t(filePath, std::ios::binary);
            t.read((char*)buf, fileSize);

            buffer = buf;
            size = fileSize;
        }

        static void WriteAllBytes(const std::wstring& filePath, const size_t size, const uint8_t* const buffer)
        {
            using namespace std;
            using namespace std::experimental;

            ofstream file(filePath, std::ios::binary);
            file.write((char*)buffer, size);
        }

    };
}
