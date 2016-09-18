#pragma once
#include <windowsx.h>
namespace OIV
{
    class TestApp
    {
    public:
        TestApp();
        void Run(std::wstring filePath);

        void JumpFiles(int step);
        
        void HandleMessages(const MSG& uMsg);
        template<class T, class U>
        bool ExecuteCommand(CommandExecute command, T * request, U * response);

    private:
        ListFilesIterator fCurrentListPosition;
        ListFiles fListFiles;
        bool LoadFile(std::wstring filePath);
        void LoadFileInFolder(std::wstring filePath);
    };
}