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
            uint32_t defaultLocalTick() {
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
            const uint32_t nowTick = localTickFn_();

            // Fast path: cache still valid AND within TTL.
            // refreshIntervalMs_ == 0 disables caching (every call refetches).
            if (cachedValid_ && refreshIntervalMs_ != 0U &&
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

            // Full 64-bit UTC epoch-ms. ITimeProvider::nowMs() now returns
            // uint64_t so no truncation is needed.
            cachedEpochMs_ = static_cast<uint64_t>(epochSec) * 1000ULL;
            cachedAnchorTick_ = nowTick;
            cachedValid_ = true;
        }

        uint64_t NtpTimeProvider::nowMs() const {
            ensureCacheFresh();
            if (!cachedValid_) {
                return 0;
            }
            // anchor + monotonic delta since anchor. The local-tick diff is
            // computed in 32-bit (correct across the unsigned wrap) and
            // promoted to 64-bit when added to the epoch anchor.
            return cachedEpochMs_ + (localTickFn_() - cachedAnchorTick_);
        }

        bool NtpTimeProvider::isValid() const {
            ensureCacheFresh();
            return cachedValid_;
        }

    }  // namespace ntp
}  // namespace ungula
