#include <gtest/gtest.h>

#include "CameraModel.hpp"
#include "CameraRepository.hpp"

namespace {

dsd::model::Camera makeCamera(const std::string& name) {
    dsd::model::Camera c;
    c.name = name;
    c.rtsp_url = "rtsp://example/" + name;
    c.enabled = true;
    return c;
}

TEST(CameraRepository, AddAssignsIdAndListsRow) {
    dsd::CameraRepository repo(":memory:");
    const dsd::model::Camera added = repo.add(makeCamera("front"));
    EXPECT_GT(added.id, 0);

    const auto all = repo.list();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].id, added.id);
    EXPECT_EQ(all[0].name, "front");
    EXPECT_EQ(all[0].rtsp_url, "rtsp://example/front");
    EXPECT_TRUE(all[0].enabled);
}

TEST(CameraRepository, UpdateModifiesRow) {
    dsd::CameraRepository repo(":memory:");
    dsd::model::Camera cam = repo.add(makeCamera("cam"));

    cam.name = "renamed";
    cam.enabled = false;
    const auto updated = repo.update(cam);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->name, "renamed");
    EXPECT_FALSE(updated->enabled);

    const auto all = repo.list();
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(all[0].name, "renamed");
    EXPECT_FALSE(all[0].enabled);
}

TEST(CameraRepository, UpdateMissingReturnsNullopt) {
    dsd::CameraRepository repo(":memory:");
    dsd::model::Camera ghost = makeCamera("ghost");
    ghost.id = 999;  // no such row
    EXPECT_FALSE(repo.update(ghost).has_value());
}

TEST(CameraRepository, RemoveDeletesRow) {
    dsd::CameraRepository repo(":memory:");
    const dsd::model::Camera cam = repo.add(makeCamera("cam"));

    EXPECT_TRUE(repo.remove(cam.id));
    EXPECT_TRUE(repo.list().empty());
    EXPECT_FALSE(repo.remove(cam.id));  // already gone -> false
}

}  // namespace
