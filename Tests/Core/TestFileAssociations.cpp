#include <catch2/catch_test_macros.hpp>
#include <OIVAppCore/FileAssociationPolicy.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

TEST_CASE("Association shortcuts resolve to the add and remove commands", "[associations][AppCore]")
{
    const auto directory = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                           "OIViewer/Resources/Configuration";
    std::ifstream bindingsFile(directory / "KeyBindings.json");
    std::ifstream commandsFile(directory / "Commands.json");
    const auto bindings = nlohmann::json::parse(bindingsFile);
    const auto commands = nlohmann::json::parse(commandsFile);
    for (const auto& [key, group, action] :
         std::array{std::array{"Control+Shift+A", "AddFileAssociations", "action=add"},
                    std::array{"Control+Shift+R", "RemoveFileAssociations", "action=remove"}})
    {
        REQUIRE(std::ranges::count_if(bindings["KeyBindings"], [&](const auto& binding)
                                      { return binding.contains(key) && binding[key] == group; }) == 1);
        REQUIRE(std::ranges::count_if(commands["commands"],
                                      [&](const auto& command)
                                      {
                                          return command["GroupID"] == group &&
                                                 command["Name"] == "cmd_file_associations" &&
                                                 command["arguments"] == action;
                                      }) == 1);
    }
}

TEST_CASE("Association prompt locations normalize alternate path spellings", "[associations][AppCore]")
{
    const auto executable = std::filesystem::current_path() / "OIViewer";
    const auto alternate  = executable.parent_path() / "." / executable.filename();
    REQUIRE(OIV::FileAssociationPolicy::ExecutableLocation(executable.native()) ==
            OIV::FileAssociationPolicy::ExecutableLocation(alternate.native()));
}
