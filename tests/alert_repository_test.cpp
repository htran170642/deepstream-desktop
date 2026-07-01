#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "AlertRepository.hpp"
#include "pipeline/Alert.hpp"

namespace {

dsd::model::Alert makeAlert(std::int64_t camera_id, const std::string& label,
                            std::int64_t ts,
                            std::vector<std::uint8_t> jpeg = {}) {
    dsd::model::Alert a;
    a.camera_id = camera_id;
    a.label = label;
    a.confidence = 0.8f;
    a.timestamp_ms = ts;
    a.snapshot = std::move(jpeg);
    return a;
}

TEST(AlertRepository, AddAssignsIdAndListsNewestFirst) {
    dsd::AlertRepository repo(":memory:");
    repo.add(makeAlert(1, "person", 1000));
    const auto second = repo.add(makeAlert(1, "car", 2000));
    EXPECT_GT(second.id, 0);

    const auto all = repo.list();
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].label, "car");     // newest first (ts 2000)
    EXPECT_EQ(all[1].label, "person");
}

TEST(AlertRepository, ListDoesNotLoadSnapshotButFlagsIt) {
    dsd::AlertRepository repo(":memory:");
    repo.add(makeAlert(1, "person", 1000, {1, 2, 3}));  // with image
    repo.add(makeAlert(1, "car", 2000));                // without image

    const auto all = repo.list();
    ASSERT_EQ(all.size(), 2u);
    for (const auto& a : all) {
        EXPECT_TRUE(a.snapshot.empty());  // list never carries the bytes
    }
    EXPECT_FALSE(all[0].has_snapshot);  // "car" -> no image
    EXPECT_TRUE(all[1].has_snapshot);   // "person" -> has image
}

TEST(AlertRepository, SnapshotReturnsBytesOrNullopt) {
    dsd::AlertRepository repo(":memory:");
    const auto with_img = repo.add(makeAlert(1, "person", 1000, {9, 8, 7}));
    const auto no_img = repo.add(makeAlert(1, "car", 2000));

    const auto bytes = repo.snapshot(with_img.id);
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, (std::vector<std::uint8_t>{9, 8, 7}));

    EXPECT_FALSE(repo.snapshot(no_img.id).has_value());  // exists, no image
    EXPECT_FALSE(repo.snapshot(9999).has_value());       // no such alert
}

TEST(AlertRepository, FilterByCameraLabelAndTime) {
    dsd::AlertRepository repo(":memory:");
    repo.add(makeAlert(1, "person", 1000));
    repo.add(makeAlert(2, "person", 2000));
    repo.add(makeAlert(1, "car", 3000));

    dsd::AlertRepository::Filter by_cam;
    by_cam.camera_id = 1;
    EXPECT_EQ(repo.list(by_cam).size(), 2u);

    dsd::AlertRepository::Filter by_label;
    by_label.label = "pers";  // substring
    EXPECT_EQ(repo.list(by_label).size(), 2u);

    dsd::AlertRepository::Filter by_time;
    by_time.from_ms = 1500;
    by_time.to_ms = 2500;
    const auto ranged = repo.list(by_time);
    ASSERT_EQ(ranged.size(), 1u);
    EXPECT_EQ(ranged[0].camera_id, 2);
}

TEST(AlertRepository, LimitCapsRowCount) {
    dsd::AlertRepository repo(":memory:");
    for (int i = 0; i < 5; ++i) {
        repo.add(makeAlert(1, "person", 1000 + i));
    }
    dsd::AlertRepository::Filter f;
    f.limit = 2;
    EXPECT_EQ(repo.list(f).size(), 2u);
}

}  // namespace
