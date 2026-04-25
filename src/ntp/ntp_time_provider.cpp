// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include "ntp_time_provider.h"

namespace ungula {
    namespace ntp {

        namespace {

            // Host-side fallback pointers. Any nullptr passed to the
            // constructor resolves to these.
            bool defaultIsSynced() {
                return ntp_is_synced();
            }
            time_t defaultEpoch() {
                return ntp_epoch();
            }
            ungula::TimeControl::tick_ms_t defaultLocalTick() {
                return ungula::TimeControl::millis();
            }

        }  // namespace

        NtpTimeProvider::NtpTimeProvider()
            : NtpTimeProvider(nullptr, nullptr, nullptr) {}

        NtpTimeProvider::NtpTimeProvider(NtpIsSyncedFn isSyncedFn, NtpEpochFn epochFn,
                                         LocalTickFn localTickFn)
            : isSyncedFn_(isSyncedFn != nullptr ? isSyncedFn : &defaultIsSynced),
              epochFn_(epochFn != nullptr ? epochFn : &defaultEpoch),
              localTickFn_(localTickFn != nullptr ? localTickFn : &defaultLocalTick) {}

        void NtpTimeProvider::ensureCacheFresh() const {
            const ungula::TimeControl::tick_ms_t nowTick = localTickFn_();

            // Fast path: cache still valid AND within TTL.
            // refreshIntervalMs_ == 0 disables caching (every call refetches).
            if (cachedValid_ && refreshIntervalMs_ != 0 &&
                (nowTick - cachedAnchorTick_) < refreshIntervalMs_) {
                return;
            }

            if (!isSyncedFn_()) {
                cachedValid_ = false;
                return;
            }

            const time_t epochSec = epochFn_();
            if (epochSec <= 0) {
                cachedValid_ = false;
                return;
            }

            // Full 64-bit UTC epoch-ms. ITimeProvider::nowMs() returns
            // TimeControl::epoch_ms_t (signed 64-bit).
            cachedEpochMs_ =
                    static_cast<ungula::TimeControl::epoch_ms_t>(epochSec) * 1000;
            cachedAnchorTick_ = nowTick;
            cachedValid_ = true;
        }

        ungula::TimeControl::epoch_ms_t NtpTimeProvider::nowMs() const {
            ensureCacheFresh();
            if (!cachedValid_) {
                return 0;
            }
            // anchor + monotonic delta since anchor. All three operands
            // are signed 64-bit (TimeControl named aliases) — diff is
            // exact, no truncation, no overflow at realistic scales.
            return cachedEpochMs_ + (localTickFn_() - cachedAnchorTick_);
        }

        bool NtpTimeProvider::isValid() const {
            ensureCacheFresh();
            return cachedValid_;
        }

    }  // namespace ntp
}  // namespace ungula
