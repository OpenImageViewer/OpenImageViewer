#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "Buffer.h"

namespace LLUtils
{
    class File
    {
    public:
        static std::string ReadAllText(std::wstring filePath)
        {
            using namespace std;
            ifstream t(filePath);
            if (t.is_open())
            {
                stringstream buffer;
                buffer << t.rdbuf();
                return buffer.str();
            }
            else
                return std::string();
        }

        static void WriteAllText(const std::wstring& filePath, const std::string& text, bool append = false)
        {
            using namespace std;
            using namespace std::experimental;

            ofstream file(filePath, append ? std::ios_base::app : std::ios_base::out);
            file << text;
        }

        static void ReadAllBytes(std::wstring filePath,std::size_t& size, uint8_t*& buffer)
        {
            using namespace std;
            std::size_t fileSize = filesystem::file_size(filePath);
            uint8_t* buf = new uint8_t[fileSize];
            ifstream t(filePath, std::ios::binary);
            t.read((char*)buf, fileSize);

            buffer = buf;
            size = fileSize;
        }

        static LLUtils::Buffer ReadAllBytes(std::wstring filePath)
        {
            using namespace std;
            std::size_t fileSize = filesystem::file_size(filePath);
            LLUtils::Buffer buf(fileSize);
            ifstream t(filePath, std::ios::binary);
            t.read((char*)buf.GetBuffer(), fileSize);
            return buf;
        }


        static void WriteAllBytes(const std::wstring& filePath, const std::size_t size, const std::byte* const buffer)
        {
            using namespace std;
            filesystem::path parent = filesystem::path(filePath).parent_path();
            if (filesystem::exists(parent) == false)
                filesystem::create_directory(parent);

            ofstream file(filePath, std::ios::binary);
            file.write((char*)buffer, size);
        }
    };
}
