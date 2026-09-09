#include "Main.h"
#include "CopyDataProtocol.h"
#include "ViewerApplication.h"

#include <LLUtils/FileSystemHelper.h>
#include <Windows.h>
#include <cstdlib>

namespace
{
    bool ForwardFile(const LLUtils::native_string_type& input)
    {
        const auto window = OIV::ViewerApplication::FindTrayBarWindow();
        if (window == 0)
            return false;
        const auto path = LLUtils::FileSystemHelper::ResolveFullPath(input);
        COPYDATASTRUCT data{};
        data.dwData = OIV::Win32::LoadFileCopyDataId;
        data.cbData = static_cast<DWORD>((path.size() + 1) * sizeof(wchar_t));
        data.lpData = const_cast<wchar_t*>(path.c_str());
        SendMessageW(reinterpret_cast<HWND>(window), WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data));
        return true;
    }

    void WriteText(HANDLE stream, const std::string& text)
    {
        if (stream == nullptr || stream == INVALID_HANDLE_VALUE || text.empty())
            return;
        DWORD mode{}, written{};
        if (GetConsoleMode(stream, &mode))
        {
            const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                                 static_cast<int>(text.size()), nullptr, 0);
            if (size > 0)
            {
                std::wstring wide(static_cast<size_t>(size), L'\0');
                MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                    wide.data(), size);
                WriteConsoleW(stream, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
            }
        }
        else
        {
            size_t offset = 0;
            while (
                offset < text.size() &&
                WriteFile(stream, text.data() + offset, static_cast<DWORD>(text.size() - offset), &written, nullptr) &&
                written != 0)
                offset += written;
        }
    }
}  // namespace

// The manifest keeps desktop launches detached while terminal launches inherit their
// console and redirected streams. No runtime console allocation or attachment is needed.
int wmain(int argc, wchar_t* argv[])
{
    OIV::CommandLineExit result;
    try
    {
        auto parsed = OIV::ParseCommandLine(argc, argv);
        if (auto* exit = std::get_if<OIV::CommandLineExit>(&parsed))
            result = std::move(*exit);
        else
            result = RunViewer(std::get<OIV::CommandLineParameters>(parsed), ForwardFile);
    }
    catch (const std::exception& error)
    {
        result = {EXIT_FAILURE, {}, std::string("OIViewer: ") + error.what() + "\n"};
    }
    WriteText(GetStdHandle(STD_OUTPUT_HANDLE), result.standardOutput);
    WriteText(GetStdHandle(STD_ERROR_HANDLE), result.standardError);
    return result.exitCode;
}
