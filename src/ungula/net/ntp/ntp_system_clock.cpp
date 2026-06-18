// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa
// See LICENSE file for details.

// ---------------------------------------------------------------
// The "make NTP the system clock" composition (ensure_system_clock). Kept in its
// own TU inside the `/ntp/` group so node builds — which exclude `/ntp/` — never
// pull in the time-provider + emblogx wiring. The STA-gated bring-up + the raw
// SNTP control stay in ntp_client; this only composes them with the lib_core
// time provider + the logger's timestamp source.
// ---------------------------------------------------------------

#include "ungula/net/ntp/ntp_client.h"
#include "ungula/net/ntp/ntp_time_provider.h"

#include <emblogx/logger.h>
#include <ungula/core/time/time.h>

namespace ungula::net::ntp
{

bool ensure_system_clock(const NtpConfig &config)
{
        if (!ensure_started(config)) {
                log_debug_m("ntp", "skipping system-clock init — STA not connected");
                return false;
        }

        // ntp_init is one-shot, so a mid-session reconnect wouldn't re-poll on its
        // own — force an immediate sync so the clock recovers in seconds.
        log_info_force_m("ntp", "STA (re)connect — forcing NTP resync");
        resync();

        // Install the NTP-backed provider as THE system clock once, then point the
        // logger at the same clock so log timestamps and the system time agree.
        static bool installed = false;
        if (!installed) {
                static ungula::net::ntp::NtpTimeProvider s_clock;
                ungula::core::time::setTimeProvider(&s_clock);
                ::emblogx::set_now_ms_provider(+[]() -> int64_t { return ungula::core::time::now(); });
                installed = true;
                log_info_force_m("ntp", "NTP client started; system time provider installed");
        }
        return true;
}

} // namespace ungula::net::ntp
