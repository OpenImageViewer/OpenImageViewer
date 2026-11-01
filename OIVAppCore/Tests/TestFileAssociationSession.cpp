#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <OIVAppCore/FileAssociationSession.h>
#include <OIVAppCore/FileAssociationPreferences.h>
#include <atomic>
#include <chrono>
#include <fstream>

namespace
{
    using Choice = OIV::FileAssociationChoice;
    using Action = OIV::FileAssociationAction;
    using Kind   = OIV::FileAssociationNotice::Kind;
    struct AssociationFixture
    {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
                                     ("OIViewer-associations-" +
                                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        // Session preferences use canonical locations even when the runner's temporary path is an alias.
        LLUtils::native_string_type executable = OIV::FileAssociationPolicy::ExecutableLocation(
            (root / "viewer").native());
        ~AssociationFixture() { std::filesystem::remove_all(root); }
    };
    bool WaitFor(const auto& predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!predicate() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return predicate();
    }
}  // namespace
TEST_CASE("Association sessions persist only permanent choices", "[associations][AppCore]")
{
    AssociationFixture fixture;
    const auto choice = GENERATE(Choice::Associate, Choice::SuppressForCopy, Choice::SuppressForUser,
                                 Choice::AskNextTime);
    std::atomic_bool prompted{};
    std::atomic_uint additions{};
    bool checkCompleted = false;
    std::atomic_bool additionAfterCheck{};
    OIV::FileAssociationSession session(
        fixture.root, fixture.executable,
        [&](Action action, std::stop_token, const OIV::FileAssociationNotify& notify)
        {
            if (action == Action::Check)
            {
                notify({Kind::Prompt, {}});
                checkCompleted = true;
            }
            else if (action == Action::Add)
            {
                ++additions;
                additionAfterCheck = checkCompleted;
            }
        },
        [&](auto notice) { prompted = notice.kind == Kind::Prompt; });
    REQUIRE(WaitFor([&] { return prompted.load(); }));
    REQUIRE_FALSE(std::filesystem::exists(fixture.root / "association-prompts.json"));
    session.Respond(choice);
    REQUIRE(WaitFor([&] { return !session.IsRunning(); }));
    REQUIRE(additions == (choice == Choice::Associate ? 1 : 0));
    // A prompt notification must return and let Check finish before the session starts Add.
    REQUIRE(additionAfterCheck == (choice == Choice::Associate));
    REQUIRE(OIV::IsAssociationPromptSuppressed(fixture.root, fixture.executable) == (choice != Choice::AskNextTime));
    REQUIRE(OIV::IsAssociationPromptSuppressed(fixture.root, OIV::FileAssociationPolicy::ExecutableLocation(
                                                                 (fixture.root / "other-copy").native())) ==
            (choice == Choice::SuppressForUser));
}
TEST_CASE("Association sessions retain prompt ownership until response or cancellation", "[associations][AppCore]")
{
    AssociationFixture fixture;
    std::atomic_uint prompts{};
    const auto backend = [](Action, std::stop_token, const OIV::FileAssociationNotify& notify)
    { notify({Kind::Prompt, {}}); };
    const auto notify = [&](auto notice)
    {
        if (notice.kind == Kind::Prompt)
            ++prompts;
    };
    {
        OIV::FileAssociationSession first(fixture.root, fixture.executable, backend, notify);
        REQUIRE(WaitFor([&] { return prompts == 1; }));
        OIV::FileAssociationSession second(fixture.root, fixture.executable, backend, notify);
        REQUIRE(WaitFor([&] { return !second.IsRunning(); }));
        REQUIRE(prompts == 1);
    }
    REQUIRE_FALSE(OIV::IsAssociationPromptSuppressed(fixture.root, fixture.executable));
    OIV::FileAssociationSession next(fixture.root, fixture.executable, backend, notify);
    REQUIRE(WaitFor([&] { return prompts == 2; }));
    next.Respond(Choice::AskNextTime);
    REQUIRE(WaitFor([&] { return !next.IsRunning(); }));
}
TEST_CASE("Association worker boundaries contain native and reporting failures", "[associations][AppCore]")
{
    AssociationFixture fixture;
    const bool nativeFailure = GENERATE(false, true);
    std::atomic_uint notifications{};
    OIV::FileAssociationSession session(
        fixture.root, fixture.executable,
        [&](Action, std::stop_token, const OIV::FileAssociationNotify& notify)
        {
            if (nativeFailure)
                throw 42;
            notify({Kind::Prompt, {}});
        },
        [&](auto)
        {
            ++notifications;
            throw std::bad_alloc();
        });
    REQUIRE(WaitFor([&] { return !session.IsRunning(); }));
    REQUIRE(notifications == (nativeFailure ? 1 : 2));
    REQUIRE_FALSE(OIV::IsAssociationPromptSuppressed(fixture.root, fixture.executable));
}
TEST_CASE("Shared association preferences preserve Unicode and reject malformed files", "[associations][AppCore]")
{
    AssociationFixture fixture;
    const auto location = OIV::FileAssociationPolicy::ExecutableLocation(
        (fixture.root / LLUTILS_TEXT("\u56fe\u50cf") / "viewer").native());
    OIV::SaveAssociationPromptChoice(fixture.root, location);
    REQUIRE(OIV::IsAssociationPromptSuppressed(fixture.root, location));
    const auto path = fixture.root / "association-prompts.json";
    const auto read = [&]
    {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    const auto previous = read();
    OIV::SaveAssociationPromptChoice(fixture.root, fixture.executable, Choice::AskNextTime);
    REQUIRE(read() == previous);
    std::ofstream(path, std::ios::binary) << "{broken}";
    REQUIRE_THROWS(OIV::IsAssociationPromptSuppressed(fixture.root, location));
    REQUIRE_THROWS(OIV::SaveAssociationPromptChoice(fixture.root, location));
    REQUIRE(read() == "{broken}");
}
