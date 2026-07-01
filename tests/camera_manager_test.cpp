#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "CameraManager.hpp"

namespace {

dsd::model::Camera makeCamera(const std::string& name, const std::string& url) {
    dsd::model::Camera c;
    c.name = name;
    c.rtsp_url = url;
    c.enabled = true;
    return c;
}

TEST(CameraManager, AddRejectsEmptyName) {
    dsd::CameraManager mgr(":memory:");
    EXPECT_THROW(mgr.add(makeCamera("", "rtsp://x")), std::invalid_argument);
}

TEST(CameraManager, AddRejectsEmptyRtspUrl) {
    dsd::CameraManager mgr(":memory:");
    EXPECT_THROW(mgr.add(makeCamera("cam", "")), std::invalid_argument);
}

TEST(CameraManager, AddAssignsIdAndPersists) {
    dsd::CameraManager mgr(":memory:");
    const auto added = mgr.add(makeCamera("cam", "rtsp://x"));
    EXPECT_GT(added.id, 0);
    ASSERT_EQ(mgr.list().size(), 1u);
    EXPECT_EQ(mgr.list().front().name, "cam");
}

TEST(CameraManager, UpdateUnknownIdReturnsNullopt) {
    dsd::CameraManager mgr(":memory:");
    dsd::model::Camera ghost = makeCamera("ghost", "rtsp://x");
    ghost.id = 999;
    EXPECT_FALSE(mgr.update(ghost).has_value());
}

TEST(CameraManager, UpdateModifiesExisting) {
    dsd::CameraManager mgr(":memory:");
    auto added = mgr.add(makeCamera("cam", "rtsp://x"));
    added.name = "renamed";
    const auto updated = mgr.update(added);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->name, "renamed");
    EXPECT_EQ(mgr.list().front().name, "renamed");
}

TEST(CameraManager, RemoveUnknownIdReturnsFalse) {
    dsd::CameraManager mgr(":memory:");
    EXPECT_FALSE(mgr.remove(12345));
}

TEST(CameraManager, RemoveDeletesExisting) {
    dsd::CameraManager mgr(":memory:");
    const auto added = mgr.add(makeCamera("cam", "rtsp://x"));
    EXPECT_TRUE(mgr.remove(added.id));
    EXPECT_TRUE(mgr.list().empty());
}

}  // namespace
