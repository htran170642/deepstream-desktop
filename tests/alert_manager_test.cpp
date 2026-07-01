#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "AlertManager.hpp"
#include "AlertRepository.hpp"
#include "pipeline/Alert.hpp"
#include "pipeline/Frame.hpp"

namespace {

dsd::model::Frame makeFrame(std::int64_t camera_id, const std::string& label,
                            float conf, std::int64_t ts,
                            std::vector<std::uint8_t> jpeg = {}) {
    dsd::model::Frame f;
    f.camera_id = camera_id;
    f.timestamp_ms = ts;
    f.width = 1920;
    f.height = 1080;
    f.jpeg = std::move(jpeg);
    dsd::model::Detection d;
    d.camera_id = camera_id;
    d.label = label;
    d.confidence = conf;
    d.timestamp_ms = ts;
    f.detections.push_back(d);
    return f;
}

TEST(AlertManager, FiresAboveThresholdAndPersists) {
    dsd::AlertRepository repo(":memory:");
    // `fired` before `manager`: ~AlertManager drains its worker (which calls the
    // sink), so the sink's captures must outlive the manager.
    std::vector<std::shared_ptr<const dsd::model::Alert>> fired;
    dsd::AlertManager manager(repo);
    manager.setAlertSink([&](auto a) { fired.push_back(std::move(a)); });

    manager.onFrame(makeFrame(1, "person", 0.9f, 1000));
    manager.flush();  // persistence is async; wait for the worker to drain

    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0]->label, "person");
    EXPECT_GT(fired[0]->id, 0);           // persisted -> has an id
    EXPECT_EQ(repo.list().size(), 1u);
}

TEST(AlertManager, BelowThresholdIgnored) {
    dsd::AlertRepository repo(":memory:");
    dsd::AlertManager::Config cfg;
    cfg.min_confidence = 0.5f;
    dsd::AlertManager manager(repo, cfg);

    manager.onFrame(makeFrame(1, "person", 0.3f, 1000));
    manager.flush();
    EXPECT_TRUE(repo.list().empty());
}

TEST(AlertManager, NonTargetClassIgnored) {
    dsd::AlertRepository repo(":memory:");
    dsd::AlertManager::Config cfg;
    cfg.targets = {"car"};  // only cars alert
    dsd::AlertManager manager(repo, cfg);

    manager.onFrame(makeFrame(1, "person", 0.9f, 1000));
    manager.flush();
    EXPECT_TRUE(repo.list().empty());
}

TEST(AlertManager, CooldownSuppressesDuplicates) {
    dsd::AlertRepository repo(":memory:");
    dsd::AlertManager::Config cfg;
    cfg.cooldown_ms = 10000;
    dsd::AlertManager manager(repo, cfg);

    manager.onFrame(makeFrame(1, "person", 0.9f, 1000));  // fires
    manager.onFrame(makeFrame(1, "person", 0.9f, 5000));  // within cooldown -> no
    manager.flush();
    EXPECT_EQ(repo.list().size(), 1u);

    manager.onFrame(makeFrame(1, "person", 0.9f, 12000)); // past cooldown -> fires
    manager.flush();
    EXPECT_EQ(repo.list().size(), 2u);
}

TEST(AlertManager, CooldownIsPerCameraAndLabel) {
    dsd::AlertRepository repo(":memory:");
    dsd::AlertManager manager(repo);  // default cooldown 10s

    manager.onFrame(makeFrame(1, "person", 0.9f, 1000));
    manager.onFrame(makeFrame(2, "person", 0.9f, 1000));  // other camera -> fires
    manager.onFrame(makeFrame(1, "car", 0.9f, 1000));     // other label  -> fires
    manager.flush();
    EXPECT_EQ(repo.list().size(), 3u);
}

TEST(AlertManager, SnapshotCopiedFromFrame) {
    dsd::AlertRepository repo(":memory:");
    dsd::AlertManager manager(repo);

    manager.onFrame(makeFrame(1, "person", 0.9f, 1000, {4, 2}));
    manager.flush();

    const auto all = repo.list();
    ASSERT_EQ(all.size(), 1u);
    ASSERT_TRUE(all[0].has_snapshot);
    const auto bytes = repo.snapshot(all[0].id);
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, (std::vector<std::uint8_t>{4, 2}));
}

}  // namespace
