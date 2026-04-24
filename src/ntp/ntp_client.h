// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

/// @brief NTP time source — raw UTC epoch seconds, nothing else.
///
/// Single responsibility: bring up the SNTP service, report whether it
/// has synced, and hand back the current UTC epoch when asked. Timezone
/// offsetting and string formatting belong to `TimeControl` and
/// `time_format` respectively — those concerns are independent of the
/// time source (they apply equally to NTP, an RTC chip, a manual
/// `setTime()`, or a fake).
///
/// On ESP32 this is a thin wrapper over the ESP-IDF SNTP service, which
/// updates the POSIX system clock in the background. On desktop hosts
/// the API is stubbed (always returns "not synced") so projects compile
/// without changes.

#include <cstdint>
#include <ctime>

namespace ungula {
    namespace ntp {

        struct NtpConfig {
                const char* server = "pool.ntp.org";
                const char* fallbackServer = "time.google.com";
                uint32_t syncIntervalSec = 3600;  // re-sync interval (default 1 h)
        };

        /// Start the SNTP service. Safe to call more than once (subsequent
        /// calls are ignored). WiFi STA must be connected before calling
        /// this so the DNS resolver can reach the NTP server.
        void ntp_init(const NtpConfig& config = NtpConfig{});

        /// Stop the SNTP service and release resources.
        void ntp_stop();

        /// Returns true once the system clock has been set by NTP at
        /// least once.
        bool ntp_is_synced();

        /// Current UTC epoch (seconds since 1970-01-01 00:00:00 UTC).
        /// Returns 0 if the clock has not been set yet.
        time_t ntp_epoch();

    }  // namespace ntp
}  // namespace ungula
