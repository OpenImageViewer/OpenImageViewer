#include "FileAssociationPanel.h"
#include "FileAssociations.h"
#include <OIVAppCore/FileLock.h>
#include <LLUtils/PlatformUtility.h>
#include <LWSUI/GridView.hpp>
#include <LWSUI/UIHost.hpp>
#include <LWS/Timer.hpp>
#include <LWS/Platform.hpp>
#include <future>
#include <thread>

namespace OIV
{
    namespace
    {
#ifdef _WIN32
        namespace Backend = Win32;
#else
        namespace Backend = Linux;
#endif
        // The grids fill their row; keep the transfer buttons centered within that same height.
        class CenteredContent final : public LWSUI::Container
        {
          public:

            explicit CenteredContent(std::unique_ptr<LWSUI::Control> content) { Add(std::move(content)); }

          protected:

            LWSUI::Size OnMeasure(LWSUI::Size available) override
            {
                Children().front()->Measure(available);
                return available;
            }
            void OnArrange() override
            {
                const auto bounds  = Bounds();
                auto& content      = *Children().front();
                const float height = std::min(bounds.height, content.DesiredSize().height);
                content.Arrange({bounds.x, bounds.y + (bounds.height - height) / 2, bounds.width, height});
            }
        };
        struct QueryResult
        {
            std::vector<FileAssociationEntry> entries;
            std::string error;
        };
        class FileAssociationPanel final : public LWSUI::Container
        {
          public:

            FileAssociationPanel(LWS::PlatformContext& platform, std::filesystem::path config,
                                 std::set<std::string> extensions, FileAssociationModel& model)
                : fConfig(std::move(config)), fExtensions(std::move(extensions)), fModel(model), fTimer(platform)
            {
                // Extensions need a compact list, independently of the settings form width.
                SetMaxWidth(368);
                auto layout            = std::make_unique<LWSUI::DockPanel>();
                layout->gap            = 10;
                auto buttons           = std::make_unique<LWSUI::StackPanel>();
                buttons->orientation   = LWSUI::Orientation::Horizontal;
                buttons->sizeToContent = true;
                fConnections.Connect(buttons->Emplace<LWSUI::Button>("Refresh").OnClick, [this] { Start(false); });
                fConnections.Connect(buttons->Emplace<LWSUI::Button>("Discard pending").OnClick,
                                     [this]
                                     {
                                         fModel.Discard();
                                         Host()->Post(Handle(), [](LWSUI::Control& c)
                                                      { static_cast<FileAssociationPanel&>(c).Refill(); });
                                     });
#ifdef _WIN32
                fConnections.Connect(buttons->Emplace<LWSUI::Button>("System defaults").OnClick,
                                     [this]
                                     {
                                         if (!Backend::OpenFileAssociationSettings())
                                             fStatus->SetText("Open Windows Settings > Apps > Default apps.");
                                     });
#endif
                layout->AddDocked(std::move(buttons), LWSUI::Dock::Top);
                auto status = std::make_unique<LWSUI::Label>();
                fStatus     = status.get();
                layout->AddDocked(std::move(status), LWSUI::Dock::Bottom);
                auto columns     = std::make_unique<LWSUI::Grid>();
                columns->columns = {-1, 84, -1};
                columns->spacing = 12;
                for (int side = 0; side < 2; ++side)
                {
                    auto grid     = std::make_unique<LWSUI::GridView>();
                    fGrids[side]  = grid.get();
                    fModels[side] = std::make_shared<LWSUI::MemoryGridModel>(
                        std::vector<LWSUI::GridModelColumn>{{.id = "extension", .editable = false},
                                                            {.id = "label", .editable = false}});
                    BindList(side);
                    auto options = grid->Options();
                    grid->SetAppearance({.headerFont = LWSUI::FontSpec{12}});
                    options.headerHeight      = 28;
                    options.rowHeight         = 28;
                    options.striped           = true;
                    options.readOnly          = true;
                    options.multipleSelection = true;
                    options.sorting = options.reordering = options.resizing = false;
                    options.verticalLines = options.horizontalLines = false;
                    grid->SetOptions(options);
                    fConnections.Connect(grid->OnSelectionChanged, [this](const auto&) { UpdateMoves(); });
                    if (side == 1)
                    {
                        auto moves           = std::make_unique<LWSUI::StackPanel>();
                        moves->sizeToContent = true;
                        moves->spacing       = 8;
                        fMoves[0]            = &moves->Emplace<LWSUI::Button>("Add >");
                        fMoves[1]            = &moves->Emplace<LWSUI::Button>("< Remove");
                        for (int source = 0; source < 2; ++source)
                            fConnections.Connect(fMoves[source]->OnClick, [this, source] { Move(source); });
                        columns->Emplace<CenteredContent>(std::move(moves));
                    }
                    columns->Add(std::move(grid));
                }
                layout->AddDocked(std::move(columns), LWSUI::Dock::Fill);
                fLayout = &Add(std::move(layout));
                fTimer.SetCallback([this] { Poll(); });
                Refill();
            }
            ~FileAssociationPanel() override
            {
                fTimer.SetInterval(0);
                fWorker.request_stop();
            }
            void Execute(LWSSettings::CustomOperation operation, LWSSettings::CustomCompletion complete)
            {
                if (operation == LWSSettings::CustomOperation::Discard)
                {
                    fModel.Discard();
                    Refill();
                    complete({});
                }
                else if (fResult.valid())
                    complete(std::unexpected(
                        LWSSettings::Error{"associations", "File associations are still loading. Try again."}));
                else if (fModel.PendingCount() == 0)
                    complete({});
                else
                {
                    Start(true);
                    fComplete = std::move(complete);
                }
            }

          protected:

            LWSUI::Size OnMeasure(LWSUI::Size available) override
            {
                fLayout->Measure({available.width, 264});
                return {available.width, 264};
            }
            void OnArrange() override { fLayout->Arrange(Bounds()); }
            void OnAttach() override
            {
                // UIHost may update synchronously while SettingsEditor::Show is still opening.
                // A platform task waits until the opening action releases its prompt session lock.
                if (fTimer.SetTargetWindow(&Host()->Window()) != LWS::Result::Success ||
                    Host()->Window().GetPlatformContext().PostTask(
                        [handle = Handle()]
                        {
                            if (auto* panel = static_cast<FileAssociationPanel*>(handle.Get()); panel && panel->Host())
                                panel->Start(false);
                        }) != LWS::Result::Success)
                    fStatus->SetText("Could not initialize association completion delivery.");
            }
            void OnDetach() override
            {
                fTimer.SetInterval(0);
                fWorker.request_stop();
            }

          private:

            void UpdateMoves()
            {
                for (int side = 0; side < 2; ++side)
                    if (fMoves[side])
                        fMoves[side]->SetEnabled(
                            std::ranges::any_of(fGrids[side]->Selection(),
                                                [&](const auto& cell)
                                                {
                                                    const auto* entry = fModel.Find(std::get<std::string>(
                                                        fModels[side]->Value({cell.row, "extension"})));
                                                    return entry && entry->observed != FileAssociationStatus::Unknown;
                                                }));
            }
            void Move(int side)
            {
                // Refill changes grid structure; defer until selection/input dispatch has finished.
                const auto selection = fGrids[side]->Selection();
                for (const auto& cell : selection)
                {
                    fModel.Select(std::get<std::string>(fModels[side]->Value({cell.row, "extension"})), side == 0);
                }
                Host()->Post(Handle(),
                             [](LWSUI::Control& control) { static_cast<FileAssociationPanel&>(control).Refill(); });
            }
            void BindList(int side)
            {
                fGrids[side]->SetModel(fModels[side]);
                fGrids[side]->SetColumns({{.id        = "label",
                                           .header    = side == 0 ? "Unassociated" : "Associated",
                                           .widthMode = LWSUI::GridWidthMode::Proportional,
                                           .width     = 1}});
            }
            void Refill()
            {
                const auto& entries = fModel.Entries();
                if (!entries.empty() ||
                    std::ranges::any_of(fModels, [](const auto& model) { return model->RowCount() != 0; }))
                {
                    // Detach during bulk changes to avoid rebuilding the projection after every row.
                    LWSUI::Size offsets[2];
                    for (int side = 0; side < 2; ++side)
                    {
                        offsets[side] = fGrids[side]->ScrollOffset();
                        fGrids[side]->SetModel({});
                    }
                    for (const auto& model : fModels)
                        while (model->RowCount())
                            model->RemoveRow(model->RowId(model->RowCount() - 1));
                    for (const auto& entry : entries)
                    {
                        auto text = "." + entry.extension;
                        if (entry.observed == FileAssociationStatus::Unknown)
                            text += " (unavailable)";
                        else if (entry.Pending())
                            text += " *";
                        fModels[entry.Associated() ? 1 : 0]->AddRow({entry.extension, std::move(text)});
                    }
                    for (int side = 0; side < 2; ++side)
                    {
                        BindList(side);
                        fGrids[side]->SetScrollOffset(offsets[side].width, offsets[side].height);
                    }
                }
                UpdateMoves();
                fStatus->SetText(std::to_string(fModel.PendingCount()) +
                                 " pending. Shared MIME extensions move together.");
            }
            void Start(bool apply)
            {
                if (fResult.valid())
                    return;
                std::packaged_task<QueryResult(std::stop_token)> task(
                    [apply, config = fConfig, extensions = fExtensions,
                     entries    = apply ? fModel.Entries() : std::vector<FileAssociationEntry>{},
                     executable = LLUtils::PlatformUtility::GetExePath()](std::stop_token stop)
                    {
                        QueryResult result;
                        try
                        {
                            std::filesystem::create_directories(config);
                            FileLock lock(config / "associations.lock");
                            if (!lock.Lock(true, false))
                                result.error = "Another viewer is managing file associations.";
                            else
                            {
                                // Re-query even when a write throws after making partial progress.
                                try
                                {
                                    if (apply)
                                        result.error = Backend::UpdateFileAssociations(config, entries, executable,
                                                                                       stop);
                                }
                                catch (const std::exception& error)
                                {
                                    result.error = error.what();
                                }
                                catch (...)
                                {
                                    result.error = "The association operation failed.";
                                }
                                result.entries = Backend::ReadFileAssociations(extensions, executable);
                            }
                        }
                        catch (const std::exception& error)
                        {
                            result.error = error.what();
                        }
                        catch (...)
                        {
                            result.error = "The association operation failed.";
                        }
                        return result;
                    });
                auto future = task.get_future();
                std::jthread worker(std::move(task));
                fResult = std::move(future);
                fWorker = std::move(worker);
                fLayout->SetEnabled(false);
                fStatus->SetText(apply ? "Applying system associations..." : "Reading system associations...");
                fTimer.SetInterval(30);
            }
            void Poll()
            {
                if (!fResult.valid() || fResult.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    return;
                fTimer.SetInterval(0);
                auto result = fResult.get();
                if (!result.entries.empty() || fExtensions.empty())
                    fModel.Refresh(std::move(result.entries));
                fLayout->SetEnabled(true);
                Refill();
                if (result.error.empty() && fComplete && fModel.PendingCount())
                    result.error = "Some defaults were not accepted. Refresh or choose them in system Default Apps.";
                if (!result.error.empty())
                    fStatus->SetText(result.error);
                if (auto complete = std::exchange(fComplete, {}))
                    complete(result.error.empty() ? LWSSettings::Status{}
                                                  : std::unexpected(LWSSettings::Error{"associations", result.error}));
            }
            std::filesystem::path fConfig;
            std::set<std::string> fExtensions;
            FileAssociationModel& fModel;
            LWSUI::Control* fLayout;
            LWSUI::Label* fStatus;
            LWSUI::GridView* fGrids[2]{};
            LWSUI::Button* fMoves[2]{};
            std::shared_ptr<LWSUI::MemoryGridModel> fModels[2];
            LWSUI::ConnectionSet fConnections;
            LWSSettings::CustomCompletion fComplete;
            LWS::Timer fTimer;
            std::future<QueryResult> fResult;
            std::jthread fWorker;
        };
    }  // namespace
    LWSSettings::CustomContent MakeFileAssociationPanel(LWS::PlatformContext& platform,
                                                        std::filesystem::path configDirectory,
                                                        std::set<std::string> extensions, FileAssociationModel& model)
    {
        auto panel = std::make_unique<FileAssociationPanel>(platform, std::move(configDirectory), std::move(extensions),
                                                            model);
        auto* control = panel.get();
        return {std::move(panel),
                [control](auto operation, auto complete) { control->Execute(operation, std::move(complete)); }};
    }
}  // namespace OIV
