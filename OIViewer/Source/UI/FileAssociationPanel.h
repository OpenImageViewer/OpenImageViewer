#pragma once
#include <LWSSettings/SettingsView.hpp>
#include <OIVAppCore/FileAssociationModel.h>
#include <filesystem>
#include <set>

namespace OIV
{
    // The controller owns the model and must outlive the returned control.
    LWSSettings::CustomContent MakeFileAssociationPanel(LWS::PlatformContext& platform,
                                                        std::filesystem::path configDirectory,
                                                        std::set<std::string> extensions, FileAssociationModel& model);
}  // namespace OIV
