// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <ctime>
#include <cstdint>

#include <time/i_time_provider.h>
#include <time/time_control.h>

#include "ntp_client.h"

/// @brief ITimeProvider that routes TimeControl::now() through the NTP
/// client.
///
/// One-line install — host projects that already call `ntp_init()` just
/// construct this and hand it to TimeControl:
///
/// ```cpp
///   ungula::ntp::ntp_init();                   // start SNTP (existing call)
///   static ungula::ntp::NtpTimeProvider clock; // lives for program lifetime
///   ungula::TimeControl::setTimeProvider(&clock);
///   // TimeControl::now() now returns NTP-aligned 64-bit UTC epoch-ms
/// ```
///
/// ## Return value
///
/// `nowMs()` returns full 64-bit UTC epoch-ms. Subtracting two values
/// gives the correct elapsed time. Timezone shifting and string
/// formatting are not this class's concern — see
/// `TimeControl::nowLocal()` / `nowInTz()` and
/// `TimeControl::formatLocal()` / `formatUtc()`.
///
/// ## Caching
///
/// `ntp_epoch()` is already cheap (reads the kernel POSIX clock, which
/// the SNTP background task keeps updated). This provider still caches:
/// it anchors on one `ntp_epoch()` read, then for the next
/// `refreshIntervalMs` milliseconds replies via pure arithmetic
/// (`anchor_epoch_ms + (millis() - anchor_tick)`). After the TTL expires
/// it re-anchors on the next call, absorbing whatever drift SNTP has
/// corrected in the background. Default TTL 60 s — one `time()` syscall
/// per minute regardless of how hard the hot path calls `now()`.
///
/// ## Thread safety
///
/// None. Install at boot from a single context, then read from anywhere.
/// Matches the rest of TimeControl / ITimeProvider.

namespace ungula {
    namespace ntp {

        /// Function-pointer seams used by the provider. Defaulted to the
        /// real ntp_client / TimeControl API. Tests override them to
        /// inject a fake clock without touching the production path.
        ///
        /// Types follow UngulaCore's TimeControl conventions:
        ///   - `time_t` for POSIX epoch seconds (NTP's native unit)
        ///   - `tick_ms_t` for monotonic ms-since-boot (a moment in time)
        /// All time values flowing through this provider are int64_t —
        /// see TimeControl's documentation for the rationale.
        using NtpIsSyncedFn = bool (*)();
        using NtpEpochFn = time_t (*)();
        using LocalTickFn = ungula::TimeControl::tick_ms_t (*)();

        class NtpTimeProvider final : public ungula::ITimeProvider {
            public:
                /// Construct with the real NTP backend. This is what host
                /// projects use.
                NtpTimeProvider();

                /// Construct with injected sources. Intended for tests.
                /// Any function pointer left null falls back to the real
                /// backend, so tests only need to supply the seams they
                /// actually want to script.
                NtpTimeProvider(NtpIsSyncedFn isSyncedFn, NtpEpochFn epochFn,
                                LocalTickFn localTickFn);

                ungula::TimeControl::epoch_ms_t nowMs() const override;
                bool isValid() const override;

                /// Override the cache TTL. Applies to the next cache miss.
                /// Use 0 to disable caching (every call re-reads NTP).
                void setRefreshIntervalMs(ungula::TimeControl::duration_ms_t intervalMs) {
                    refreshIntervalMs_ = intervalMs;
                }

                ungula::TimeControl::duration_ms_t refreshIntervalMs() const {
                    return refreshIntervalMs_;
                }

            private:
                NtpIsSyncedFn isSyncedFn_;
                NtpEpochFn epochFn_;
                LocalTickFn localTickFn_;
                ungula::TimeControl::duration_ms_t refreshIntervalMs_ = 60'000;

                // Mutable cache — nowMs() is logically const, but the
                // cache needs to update on calls.
                mutable ungula::TimeControl::epoch_ms_t cachedEpochMs_ = 0;
                mutable ungula::TimeControl::tick_ms_t cachedAnchorTick_ = 0;
                mutable bool cachedValid_ = false;

                void ensureCacheFresh() const;
        };

    }  // namespace ntp
}  // namespace ungula
