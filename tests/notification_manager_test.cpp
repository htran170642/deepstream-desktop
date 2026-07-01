#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "notify/NotificationChannel.hpp"
#include "notify/NotificationManager.hpp"
#include "pipeline/Alert.hpp"

namespace {

// Records every send() without touching the network. send() runs on the
// manager's worker thread; the test reads counters only after flush(), whose
// lock establishes visibility, so an atomic counter is all we need.
class FakeChannel : public dsd::NotificationChannel {
public:
    FakeChannel(std::string name, bool enabled) : name_(std::move(name)),
                                                   enabled_(enabled) {}

    std::string name() const override { return name_; }
    bool enabled() const override { return enabled_; }

    bool send(const dsd::model::Alert& alert) override {
        ++calls_;
        last_label_ = alert.label;
        if (throw_on_send_) throw std::runtime_error("boom");
        return return_value_;
    }

    int calls() const { return calls_.load(); }

    std::string name_;
    bool enabled_;
    bool throw_on_send_ = false;
    bool return_value_ = true;
    std::atomic<int> calls_{0};
    std::string last_label_;
};

dsd::model::Alert makeAlert(std::int64_t id, const std::string& label) {
    dsd::model::Alert a;
    a.id = id;
    a.camera_id = 1;
    a.label = label;
    a.confidence = 0.9f;
    a.timestamp_ms = 1000;
    return a;
}

// Build a manager over the given channels, keeping raw pointers to inspect
// them after they've been moved into the manager.
dsd::NotificationManager makeManager(std::vector<FakeChannel*> raws,
                                     std::vector<std::unique_ptr<FakeChannel>> owned) {
    std::vector<std::unique_ptr<dsd::NotificationChannel>> channels;
    for (auto& ch : owned) channels.push_back(std::move(ch));
    (void)raws;
    return dsd::NotificationManager(std::move(channels));
}

TEST(NotificationManager, FansOutToAllEnabledChannels) {
    auto a = std::make_unique<FakeChannel>("a", true);
    auto b = std::make_unique<FakeChannel>("b", true);
    FakeChannel* pa = a.get();
    FakeChannel* pb = b.get();

    std::vector<std::unique_ptr<dsd::NotificationChannel>> channels;
    channels.push_back(std::move(a));
    channels.push_back(std::move(b));
    dsd::NotificationManager mgr(std::move(channels));

    mgr.notify(std::make_shared<const dsd::model::Alert>(makeAlert(1, "person")));
    mgr.flush();

    EXPECT_EQ(pa->calls(), 1);
    EXPECT_EQ(pb->calls(), 1);
    EXPECT_EQ(pa->last_label_, "person");
}

TEST(NotificationManager, SkipsDisabledChannels) {
    auto on = std::make_unique<FakeChannel>("on", true);
    auto off = std::make_unique<FakeChannel>("off", false);
    FakeChannel* pon = on.get();
    FakeChannel* poff = off.get();

    std::vector<std::unique_ptr<dsd::NotificationChannel>> channels;
    channels.push_back(std::move(on));
    channels.push_back(std::move(off));
    dsd::NotificationManager mgr(std::move(channels));

    mgr.notify(std::make_shared<const dsd::model::Alert>(makeAlert(1, "car")));
    mgr.flush();

    EXPECT_EQ(pon->calls(), 1);
    EXPECT_EQ(poff->calls(), 0);  // disabled channel never touched
}

TEST(NotificationManager, OneChannelFailureDoesNotBlockOthers) {
    auto bad = std::make_unique<FakeChannel>("bad", true);
    auto good = std::make_unique<FakeChannel>("good", true);
    bad->throw_on_send_ = true;  // first channel throws
    FakeChannel* pbad = bad.get();
    FakeChannel* pgood = good.get();

    std::vector<std::unique_ptr<dsd::NotificationChannel>> channels;
    channels.push_back(std::move(bad));
    channels.push_back(std::move(good));
    dsd::NotificationManager mgr(std::move(channels));

    mgr.notify(std::make_shared<const dsd::model::Alert>(makeAlert(1, "person")));
    mgr.flush();

    EXPECT_EQ(pbad->calls(), 1);   // attempted
    EXPECT_EQ(pgood->calls(), 1);  // and the throw didn't skip this one
}

TEST(NotificationManager, DeliversEveryQueuedAlert) {
    auto ch = std::make_unique<FakeChannel>("a", true);
    FakeChannel* pch = ch.get();

    std::vector<std::unique_ptr<dsd::NotificationChannel>> channels;
    channels.push_back(std::move(ch));
    dsd::NotificationManager mgr(std::move(channels));

    for (int i = 0; i < 50; ++i) {
        mgr.notify(std::make_shared<const dsd::model::Alert>(makeAlert(i, "person")));
    }
    mgr.flush();

    EXPECT_EQ(pch->calls(), 50);
}

}  // namespace
