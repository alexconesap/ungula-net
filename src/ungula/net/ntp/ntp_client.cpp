// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#include "ntp_client.h"

#include "ungula/net/wifi/wifi_sta.h"

// =============================================================================
// ESP-IDF implementation — uses the built-in SNTP service
// =============================================================================
#if defined(ESP_PLATFORM)

#include <esp_sntp.h>
#include <sys/time.h>

static bool s_initialised = false;

namespace ungula::net::ntp
{

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
// =============================================================================
// Desktop stub — no real NTP, always unsynchronised
// =============================================================================
#else

namespace ungula::net::ntp
{

void ntp_init(const NtpConfig & /*config*/)
{
}
void ntp_stop()
{
}
void resync()
{
}
bool ntp_is_synced()
{
        return false;
}
time_t ntp_epoch()
{
        return 0;
}

} // namespace ungula::net::ntp
#endif // ESP_PLATFORM

// ---------------------------------------------------------------------------
// Platform-agnostic convenience (uses the STA + NTP entry points above, each
// of which is platform-branched in its own translation unit).
// ---------------------------------------------------------------------------
namespace ungula::net::ntp
{

bool ensure_started(const NtpConfig &config)
{
        if (!ungula::net::wifi::sta_is_connected()) {
                return false;
        }
        ntp_init(config);
        return true;
}

} // namespace ungula::net::ntp
