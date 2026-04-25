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
            int64_t defaultLocalTick() {
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
            const int64_t nowTick = localTickFn_();

            // Fast path: cache still valid AND within TTL.
            // refreshIntervalMs_ == 0 disables caching (every call refetches).
            if (cachedValid_ && refreshIntervalMs_ != 0U &&
                (nowTick - cachedAnchorTick_) <
                        static_cast<int64_t>(refreshIntervalMs_)) {
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

            // Full 64-bit UTC epoch-ms. ITimeProvider::nowMs() returns int64_t.
            cachedEpochMs_ = static_cast<int64_t>(epochSec) * 1000;
            cachedAnchorTick_ = nowTick;
            cachedValid_ = true;
        }

        int64_t NtpTimeProvider::nowMs() const {
            ensureCacheFresh();
            if (!cachedValid_) {
                return 0;
            }
            // anchor + monotonic delta since anchor. Both int64_t — no
            // overflow concerns and the diff is exactly the right value.
            return cachedEpochMs_ + (localTickFn_() - cachedAnchorTick_);
        }

        bool NtpTimeProvider::isValid() const {
            ensureCacheFresh();
            return cachedValid_;
        }

    }  // namespace ntp
}  // namespace ungula
