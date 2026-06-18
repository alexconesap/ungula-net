// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

#pragma once

/// @brief NTP time source — raw UTC epoch seconds, nothing else.
///
/// Single responsibility: bring up the SNTP service, report whether it
/// has synced, and hand back the current UTC epoch when asked. Timezone
/// offsetting and string formatting belong to `ungula::core::time` and
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

#include "ungula/net/wifi/wifi_sta.h" // sta_is_connected (for the inline ensure_started)

// -----------------------------------------------------------------------------
// Entry point + contract. The platform implementation of the functions below
// (ntp_init / ntp_stop / resync / ntp_is_synced / ntp_epoch) lives in a
// per-platform translation unit selected at build time by a -D:
//   - esp32_ntp_client.cpp : ESP-IDF SNTP  (ESP_PLATFORM)
//   - mock_ntp_client.cpp   : host / test stub (UNGULA_NET_MOCK)
// A new platform adds its own <platform>_ntp_client.cpp — no edit here or to the
// existing ones. The platform-agnostic glue (ensure_started) is inline below.
// -----------------------------------------------------------------------------

namespace ungula::net::ntp
{

struct NtpConfig {
        const char *server = "pool.ntp.org";
        const char *fallbackServer = "time.google.com";
        uint32_t syncIntervalSec = 3600; // re-sync interval (default 1 h)
};

/// Start the SNTP service. Safe to call more than once (subsequent
/// calls are ignored). WiFi STA must be connected before calling
/// this so the DNS resolver can reach the NTP server.
void ntp_init(const NtpConfig &config = NtpConfig{});

/// Bring NTP up only when the STA link is connected: a no-op returning false
/// when the STA is down, else ntp_init() + true. The generic "start NTP once
/// we're online" both a boot path and an interactive WiFi path can call freely
/// (ntp_init is idempotent). This starts the SNTP service ONLY — it does not
/// touch the system clock or the logger. For the standard "make NTP THE system
/// wall clock + logger time source" wiring use ensure_system_clock() below; a
/// project wanting a different time source (RTC, manual setTime) skips it and
/// installs its own ITimeProvider.
///
/// Platform-agnostic (composes the STA check + ntp_init), so it is inline here
/// rather than in a per-platform .cpp.
inline bool ensure_started(const NtpConfig &config = NtpConfig{})
{
        if (!ungula::net::wifi::sta_is_connected()) {
                return false;
        }
        ntp_init(config);
        return true;
}

/// The standard "make NTP the system clock" composition every station would
/// otherwise duplicate: ensure_started() (STA-gated) + install an NtpTimeProvider
/// as the lib_core ITimeProvider + point emblogx's timestamp source at the same
/// clock + force an immediate resync. Idempotent — call it on boot and on every
/// STA (re)connect. Returns true if NTP is running or was just started; false if
/// STA is down. Lives in ntp_system_clock.cpp (part of the `/ntp/` source group
/// nodes exclude, so node builds never pull in the time-provider + logger wiring).
bool ensure_system_clock(const NtpConfig &config = NtpConfig{});

/// Force an immediate re-poll of the NTP server. `ntp_init()` is one-shot, so a
/// mid-session WiFi STA reconnect does NOT re-kick SNTP — without this the clock
/// waits up to `syncIntervalSec` (default 1 h) for the next scheduled sync. Call
/// it on every STA (re)connect so the clock recovers in seconds. No-op until NTP
/// has been started.
void resync();

/// Stop the SNTP service and release resources.
void ntp_stop();

/// Returns true once the system clock has been set by NTP at
/// least once.
bool ntp_is_synced();

/// Current UTC epoch (seconds since 1970-01-01 00:00:00 UTC).
/// Returns 0 if the clock has not been set yet.
time_t ntp_epoch();

} // namespace ungula::net::ntp
