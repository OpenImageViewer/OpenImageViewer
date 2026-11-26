#include <OIVAppCore/FileAssociationModel.h>
#include <catch2/catch_test_macros.hpp>
using namespace OIV;

TEST_CASE("Association intentions group MIME aliases and reverse without system writes", "[associations][AppCore]")
{
    FileAssociationModel model;
    model.Refresh({{"jpeg", "image/jpeg", FileAssociationStatus::OtherOrMissing},
                   {"jpg", "image/jpeg", FileAssociationStatus::OtherOrMissing},
                   {"png", "image/png", FileAssociationStatus::CurrentViewer},
                   {"unknown", "unknown", FileAssociationStatus::Unknown}});
    REQUIRE(model.Find("jpg"));
    CHECK_FALSE(model.Find("absent"));
    model.Select("jpg", true);
    CHECK(model.PendingCount() == 2);
    CHECK(model.Entries()[0].Associated());
    CHECK(model.Entries()[1].Associated());
    model.Select("jpeg", false);
    CHECK(model.PendingCount() == 0);
    model.Select("unknown", true);
    CHECK(model.PendingCount() == 0);
    model.Select("png", false);
    CHECK(model.PendingCount() == 1);
    model.Discard();
    CHECK(model.PendingCount() == 0);
    CHECK(model.Entries()[2].Associated());
}
TEST_CASE("Association refresh retains failures and clears verified successes", "[associations][AppCore]")
{
    FileAssociationModel model;
    model.Refresh(
        {{"jpg", "jpg", FileAssociationStatus::OtherOrMissing}, {"png", "png", FileAssociationStatus::OtherOrMissing}});
    model.Select("jpg", true);
    model.Select("png", true);
    model.Refresh(
        {{"jpg", "jpg", FileAssociationStatus::CurrentViewer}, {"png", "png", FileAssociationStatus::Unknown}});
    CHECK(model.PendingCount() == 1);
    REQUIRE(model.Entries()[1].desired);
    CHECK(*model.Entries()[1].desired);
    model.Refresh(
        {{"jpg", "jpg", FileAssociationStatus::CurrentViewer}, {"png", "png", FileAssociationStatus::OtherOrMissing}});
    CHECK(model.PendingCount() == 1);
    model.Refresh(
        {{"jpg", "jpg", FileAssociationStatus::CurrentViewer}, {"png", "png", FileAssociationStatus::CurrentViewer}});
    CHECK(model.PendingCount() == 0);
    model.Select("jpg", false);
    model.Discard();
    CHECK(model.Entries()[0].Associated());
}
