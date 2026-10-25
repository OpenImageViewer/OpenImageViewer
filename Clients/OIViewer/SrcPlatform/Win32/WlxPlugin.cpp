#include "WlxApi.h"
#include "WlxPreview.h"
#include "HostedInputAdapter.h"
#include "Viewer.h"
#include "ViewerResources.h"
#include <OIVAppCore/UserStorage.h>
#include <OIVAppCore/ViewerHostPolicy.h>
#include <Version.h>

#include <LWS/Win32/Platform.hpp>
#include <LWS/Win32/WindowExtensions.hpp>
#include <LLUtils/Exception.h>
#include <LLUtils/StringUtility.h>
#include <CommCtrl.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace
{
    // Legacy WLX paths use the Windows ANSI code page. LLUtils string conversions require UTF-8.
    std::wstring AnsiToWide(const char* value)
    {
        if (value == nullptr)
            return {};
        const int count = MultiByteToWideChar(CP_ACP, 0, value, -1, nullptr, 0);
        std::wstring result(count > 0 ? static_cast<size_t>(count) : 0, L'\0');
        if (count > 0)
        {
            MultiByteToWideChar(CP_ACP, 0, value, -1, result.data(), count);
            result.pop_back();
        }
        return result;
    }

}  // namespace

namespace OIV
{
    // Viewer operations use the host UI thread and native mouse events without changing its process-wide
    // raw input registration. Previews are independent and can execute on thumbnail worker threads.
    class WlxSession
    {
        struct Entry
        {
            explicit Entry(LWS::PlatformContext& platform)
                : container(platform),
                  session(CreateViewerFileWatcher(), [&platform](std::move_only_function<void()> task)
                          { return platform.PostTask(std::move(task)) == LWS::Result::Success; })
            {
            }
            // Own the plugin's child, not a wrapper around the file manager's window. Native child events
            // already supply resize and teardown. A borrowed host would add observer and read-only lifetime
            // policy to every LWS window without helping Linux, where GTK/Qt hosting needs a toolkit adapter.
            LWS::Window container;
            ViewerSession session;
            // Native subclass callbacks can run during Viewer destruction. The adapter must survive that teardown.
            std::unique_ptr<Win32::HostedInputAdapter> input;
            std::unique_ptr<Viewer> viewer;
            LWS::EventConnection connection;
            uint32_t pressedButtons{};
        };

      public:

        WlxSession()
        {
            if (LWS::Win32::BootstrapProcess({LWS::Win32::DpiPolicy::AdoptHost}) != LWS::Result::Success ||
                platform.Init({.backend = LWS::BackendId::Win32, .dispatchMode = LWS::DispatchMode::ExternalLoop}) !=
                    LWS::Result::Success)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to initialize hosted LWS");
        }

        ~WlxSession()
        {
            entries.clear();
            std::ignore = platform.Shutdown();
        }

        bool OnThread() const { return platform.IsCurrentThread(); }
        bool Empty() const { return entries.empty(); }

        HWND Load(HWND parent, const wchar_t* file, int flags)
        {
            if (!OnThread() || parent == nullptr || GetWindowThreadProcessId(parent, nullptr) != GetCurrentThreadId() ||
                file == nullptr || *file == L'\0')
                return nullptr;
            auto entry = std::make_unique<Entry>(platform);
            if (LWS::Win32::CreateChild(entry->container, parent,
                                        {.position = LWS::Point{0, 0}, .clientSize = {1, 1}, .visible = true}) !=
                LWS::Result::Success)
                return nullptr;
            const HWND handle = *LWS::Win32::GetHwnd(entry->container);
            // WLX expects an exact initial pixel fit. Integer logical dimensions can round by one pixel
            // at fractional DPI, so native placement remains at this platform boundary.
            RECT rect{};
            GetClientRect(parent, &rect);
            MoveWindow(handle, 0, 0, std::max<LONG>(rect.right, 1), std::max<LONG>(rect.bottom, 1), FALSE);
            entry->viewer = std::make_unique<Viewer>(platform, entry->session, storage.Directories(), resources,
                                                     ViewerShell{
                                                         .close =
                                                             [borrowed = entry.get(), parent](bool)
                                                         {
                                                             auto& window = borrowed->viewer->GetWindow();
                                                             if (window.GetWindowMode() != LWS::WindowMode::Windowed)
                                                                 std::ignore = window.SetWindowMode(
                                                                     LWS::WindowMode::Windowed);
                                                             else
                                                                 PostMessageW(parent, WM_KEYDOWN, VK_ESCAPE, 1);
                                                         },
                                                         .forwardKey =
                                                             [parent](uint32_t message, uint32_t key, uint32_t flags)
                                                         {
                                                             // Posting lets the current callback unwind before a host
                                                             // action can unload the DLL.
                                                             PostMessageW(parent, message, key, flags);
                                                         },
                                                         .commandAllowed = IsHostedCommandAllowed,
                                                     });
            auto& viewer  = *entry->viewer;
            entry->input  = std::make_unique<Win32::HostedInputAdapter>(viewer);
            viewer.Init(file, {},
                        {
                            .parent             = &entry->container,
                            .position           = LWS::Point{0, 0},
                            .clientSize         = entry->container.GetClientAreaMetrics().logical,
                            .styles             = {},
                            .backgroundColor    = LLUtils::Color(45, 45, 48),
                            .dragAndDropEnabled = platform.Supports(LWS::PlatformFeature::DragAndDrop).value_or(false),
                        });
            viewer.CompleteInitialization();
            if (!viewer.IsImageOpen())
                return nullptr;
            auto* borrowed  = entry.get();
            auto connection = entry->container.Listen(
                [borrowed](const LWS::AnyEvent& event)
                {
                    auto& viewer = *borrowed->viewer;
                    if (std::holds_alternative<LWS::EventWindowDestroying>(event))
                    {
                        viewer.Stop();
                        // An escaped composite is no longer a native descendant of the host container.
                        if (viewer.GetWindow().GetWindowMode() != LWS::WindowMode::Windowed)
                            std::ignore = viewer.GetWindow().Destroy();
                    }
                    else if (std::holds_alternative<LWS::EventClientAreaSizeChanged>(event) &&
                             viewer.GetWindow().GetWindowMode() == LWS::WindowMode::Windowed)
                        viewer.GetWindow().FitToParent();
                    else if (std::holds_alternative<LWS::EventFocusGained>(event))
                        SetFocus(*LWS::Win32::GetHwnd(viewer.GetWindow().GetWindow()));
                    return LWS::EventResponse::Unhandled;
                });
            if (!connection)
                return nullptr;
            entry->connection = std::move(*connection);
            for (auto* window : {&viewer.GetWindow().GetWindow(), &viewer.GetWindow().GetCanvasWindow()})
                if (!SetWindowSubclass(*LWS::Win32::GetHwnd(*window), InputProc, 1,
                                       reinterpret_cast<DWORD_PTR>(borrowed)))
                    LL_EXCEPTION_SYSTEM_ERROR("Unable to connect WLX focus handling");
            ApplyFlags(viewer, flags);
            entries.emplace(handle, std::move(entry));
            return handle;
        }

        // Replacement is host-initiated and follows the host's own file-list index. Independent viewer
        // browsing can already have reached this path, so a same-file request is still a valid replacement.
        // WLX has no general filename-changed notification that could synchronize those two histories.
        int LoadNext(HWND parent, HWND handle, const wchar_t* file, int flags)
        {
            auto* entry = Find(handle);
            if (entry == nullptr || parent != GetParent(handle) || file == nullptr || *file == L'\0')
                return WlxError;
            auto& viewer = *entry->viewer;
            if (!viewer.LoadFile(file,
                                 IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType))
                return WlxError;
            ApplyFlags(viewer, flags);
            return WlxOk;
        }

        void Close(HWND handle)
        {
            if (OnThread())
                entries.erase(handle);
        }

        int Command(HWND handle, int command, int parameter)
        {
            auto* entry = Find(handle);
            if (entry == nullptr)
                return WlxError;
            auto& viewer = *entry->viewer;
            switch (command)
            {
                case WlxCopy:
                    return viewer.CopyVisibleToClipBoard() == OperationResult::Success ? WlxOk : WlxError;
                case WlxSelectAll:
                    return viewer.ExecutePredefinedCommand("SelectEntireImage") ? WlxOk : WlxError;
                case WlxNewParams:
                    ApplyFlags(viewer, parameter);
                    return WlxOk;
                case WlxSetPercent:
                {
                    return viewer.ScrollToPercent(parameter) ? WlxOk : WlxError;
                }
                default:
                    return WlxError;
            }
        }

        int Notification(HWND handle, int message, WPARAM wParam, LPARAM lParam)
        {
            auto* entry = Find(handle);
            if (entry == nullptr || (message != WM_COMMAND && message != WM_NOTIFY))
                return 0;
            return static_cast<int>(SendMessageW(*LWS::Win32::GetHwnd(entry->viewer->GetWindow().GetWindow()),
                                                 static_cast<UINT>(message), wParam, lParam));
        }

      private:

        Entry* Find(HWND window)
        {
            if (!OnThread())
                return nullptr;
            const auto it = entries.find(window);
            return it != entries.end() && it->second->container.IsCreated() && !it->second->viewer->IsClosing()
                       ? it->second.get()
                       : nullptr;
        }

        static void ApplyFlags(Viewer& viewer, int flags)
        {
            const auto fit = (flags & WlxFitLargerOnly) != 0 ? Viewer::ViewFit::FitLargerOnly
                             : (flags & WlxFit) != 0         ? Viewer::ViewFit::Fit
                                                             : Viewer::ViewFit::Original;
            viewer.SetViewFit(fit, (flags & WlxCenter) != 0);
        }

        static LRESULT CALLBACK InputProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id,
                                          DWORD_PTR reference)
        {
            auto& entry = *reinterpret_cast<Entry*>(reference);
            if (message == WM_NCDESTROY)
                RemoveWindowSubclass(window, InputProc, id);
            entry.input->HandleInput(window, message, wParam, entry.pressedButtons);
            const auto result = DefSubclassProc(window, message, wParam, lParam);
            // Finish the normal button callback before releasing capture and cancelling the gesture.
            if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP ||
                 message == WM_XBUTTONUP) &&
                GetCapture() == window && entry.pressedButtons == 0)
                ReleaseCapture();
            return result;
        }

        // Both file managers share the WLX profile, but its settings, state, and cache-version lease
        // never interact with the standalone application. The settings schema and storage code are shared.
        StorageSession storage{UserDirectories::Resolve("OIViewer.WLX"), FormatFullVersion(CurrentVersion)};
        const ViewerResources resources{ResolveViewerResources(storage.Directories())};
        LWS::PlatformContext platform;
        std::map<HWND, std::unique_ptr<Entry>> entries;
    };
}  // namespace OIV

namespace
{
    std::unique_ptr<OIV::WlxSession> session;
}

extern "C"
{
    __declspec(dllexport) HWND __stdcall ListLoadW(HWND parent, wchar_t* file, int flags)
    {
        try
        {
            if (!IsWindow(parent) || GetWindowThreadProcessId(parent, nullptr) != GetCurrentThreadId())
                return nullptr;
            if (!session)
                session = std::make_unique<OIV::WlxSession>();
            const HWND result = session->Load(parent, file, flags);
            if (session->Empty())
                session.reset();
            return result;
        }
        catch (...)
        {
            if (session && session->OnThread() && session->Empty())
                session.reset();
            return nullptr;
        }
    }
    __declspec(dllexport) HWND __stdcall ListLoad(HWND parent, char* file, int flags)
    {
        try
        {
            auto wide = AnsiToWide(file);
            return ListLoadW(parent, wide.data(), flags);
        }
        catch (...)
        {
            return nullptr;
        }
    }
    __declspec(dllexport) int __stdcall ListLoadNextW(HWND parent, HWND window, wchar_t* file, int flags)
    {
        try
        {
            return session ? session->LoadNext(parent, window, file, flags) : WlxError;
        }
        catch (...)
        {
            return WlxError;
        }
    }
    __declspec(dllexport) int __stdcall ListLoadNext(HWND parent, HWND window, char* file, int flags)
    {
        try
        {
            auto wide = AnsiToWide(file);
            return ListLoadNextW(parent, window, wide.data(), flags);
        }
        catch (...)
        {
            return WlxError;
        }
    }
    __declspec(dllexport) void __stdcall ListCloseWindow(HWND window)
    {
        try
        {
            if (session && session->OnThread())
            {
                session->Close(window);
                if (session->Empty())
                    session.reset();
            }
        }
        catch (...)
        {
        }
    }
    __declspec(dllexport) int __stdcall ListSendCommand(HWND window, int command, int parameter)
    {
        try
        {
            return session ? session->Command(window, command, parameter) : WlxError;
        }
        catch (...)
        {
            return WlxError;
        }
    }
    __declspec(dllexport) int __stdcall ListNotificationReceived(HWND window, int message, WPARAM wp, LPARAM lp)
    {
        try
        {
            return session ? session->Notification(window, message, wp, lp) : 0;
        }
        catch (...)
        {
            return 0;
        }
    }
    __declspec(dllexport) void __stdcall ListGetDetectString(char* buffer, int capacity)
    {
        if (buffer == nullptr || capacity <= 0)
            return;
        buffer[0] = '\0';
        try
        {
            IMCodec::ImageLoader loader;
            // Restrict automatic selection to multimedia mode. Reserve the closing parenthesis and
            // terminator while adding whole extension rules so every nonempty result remains valid.
            constexpr std::string_view prefix = "MULTIMEDIA & (";
            std::string expression(prefix);
            for (const auto& plugin : loader.GetImageCodec().GetPluginsInfo())
                for (const auto& group : plugin.extensionCollection)
                    for (const auto& extension : group.listExtensions)
                    {
                        auto text = LLUtils::StringUtility::ToAString(extension);
                        if (text.empty() || !std::ranges::all_of(text,
                                                                 [](unsigned char ch)
                                                                 {
                                                                     return (ch >= 'a' && ch <= 'z') ||
                                                                            (ch >= 'A' && ch <= 'Z') ||
                                                                            (ch >= '0' && ch <= '9');
                                                                 }))
                            continue;
                        std::ranges::transform(
                            text, text.begin(), [](unsigned char ch)
                            { return static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 'a' + 'A' : ch); });
                        const auto term = std::format("{}EXT=\"{}\"", expression.size() == prefix.size() ? "" : "|",
                                                      text);
                        if (expression.size() + term.size() + 1 < static_cast<size_t>(capacity))
                            expression += term;
                    }
            if (expression.size() == prefix.size())
                expression.clear();
            else
                expression += ')';
            std::memcpy(buffer, expression.c_str(), expression.size() + 1);
        }
        catch (...)
        {
        }
    }
    __declspec(dllexport) HBITMAP __stdcall ListGetPreviewBitmapW(wchar_t* file, int width, int height, char*, int)
    {
        try
        {
            IMCodec::ImageLoader loader;
            return OIV::Win32::CreatePreviewBitmap(loader, file, width, height);
        }
        catch (...)
        {
            return nullptr;
        }
    }
    __declspec(dllexport) HBITMAP __stdcall ListGetPreviewBitmap(char* file, int width, int height, char* content,
                                                                 int length)
    {
        try
        {
            auto wide = AnsiToWide(file);
            return ListGetPreviewBitmapW(wide.data(), width, height, content, length);
        }
        catch (...)
        {
            return nullptr;
        }
    }
}
