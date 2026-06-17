// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

// =============================================================================
// NTP client — ESP-IDF implementation (built-in SNTP service, which updates the
// POSIX system clock in the background). Selected by -DESP_PLATFORM. Wholly
// guarded: on any other platform this translation unit is empty, so a sibling
// <platform>_ntp_client.cpp can provide the same contract without touching this
// file.
// =============================================================================
#if defined(ESP_PLATFORM)

#include "ntp_client.h"

#include <esp_sntp.h>
#include <sys/time.h>

namespace ungula::net::ntp
{
namespace
{
        bool s_initialised = false;
}

void ntp_init(const NtpConfig &config)
{
        if (s_initialised) {
                return;
        }

        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, config.server);
        if (config.fallbackServer != nullptr) {
                esp_sntp_setservername(1, config.fallbackServer);
        }
        esp_sntp_set_sync_interval(config.syncIntervalSec * 1000);
        esp_sntp_init();

        s_initialised = true;
}

void ntp_stop()
{
        if (!s_initialised) {
                return;
        }
        esp_sntp_stop();
        s_initialised = false;
}

void resync()
{
        if (!s_initialised) {
                return;
        }
        // Restart the SNTP service so it fires a request now instead of waiting
        // for the next scheduled poll — used on a WiFi reconnect.
        esp_sntp_restart();
}

bool ntp_is_synced()
{
        if (!s_initialised) {
                return false;
        }
        // After the first successful sync the system time jumps from the
        // boot epoch (Jan 1970) to a value well past the year 2020.
        time_t now = 0;
        time(&now);
        return now > 1577836800; // 2020-01-01 00:00:00 UTC
}

time_t ntp_epoch()
{
        time_t now = 0;
        time(&now);
        if (now < 1577836800) {
                return 0;
        }
        return now;
}

} // namespace ungula::net::ntp

#endif // ESP_PLATFORM
