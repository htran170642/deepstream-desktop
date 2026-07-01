#pragma once

#include <memory>
#include <vector>

#include "notify/NotificationChannel.hpp"

namespace dsd {

// Build notification channels from environment configuration (DSD_* vars).
// Channels are always constructed but only those with config present report
// enabled(); the rest are inert. Without libcurl (DSD_WITH_CURL undefined) no
// HTTP/SMTP transport exists, so an empty list is returned.
std::vector<std::unique_ptr<NotificationChannel>> buildNotificationChannels();

}  // namespace dsd
