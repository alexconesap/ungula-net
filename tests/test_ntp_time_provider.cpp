// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#include <gtest/gtest.h>

#include <ntp/ntp_time_provider.h>
#include <time/i_time_provider.h>
#include <time/time_control.h>

#include <cstdint>
#include <ctime>

namespace {

    using ungula::ITimeProvider;
    using ungula::TimeControl;
    using ungula::ntp::NtpTimeProvider;

    // Script-driven fake clock, exposed via free functions because the
    // provider takes function-pointer seams. File-static so multiple tests
    // share the scripting surface without ceremony.
    struct FakeClock {
            bool synced = false;
            time_t epoch = 0;
            int64_t localTick = 0;
    };

    FakeClock g_fake;

    bool fakeIsSynced() {
        return g_fake.synced;
    }
    time_t fakeEpoch() {
        return g_fake.epoch;
    }
    int64_t fakeLocalTick() {
        return g_fake.localTick;
    }

    // Plus a call counter on each seam so tests can assert the cache
    // actually prevents re-fetches.
    int g_isSyncedCalls = 0;
    int g_epochCalls = 0;
    int g_localTickCalls = 0;

    bool countingIsSynced() {
        ++g_isSyncedCalls;
        return g_fake.synced;
    }
    time_t countingEpoch() {
        ++g_epochCalls;
        return g_fake.epoch;
    }
    int64_t countingLocalTick() {
        ++g_localTickCalls;
        return g_fake.localTick;
    }

    class NtpTimeProviderTest : public ::testing::Test {
        protected:
            void SetUp() override {
                g_fake = {};
                g_isSyncedCalls = 0;
                g_epochCalls = 0;
                g_localTickCalls = 0;
                TimeControl::clearTimeProvider();
            }
            void TearDown() override {
                TimeControl::clearTimeProvider();
            }
    };

    // ---- Basic contract ----

    TEST_F(NtpTimeProviderTest, InvalidBeforeNtpSync) {
        NtpTimeProvider p(&fakeIsSynced, &fakeEpoch, &fakeLocalTick);
        EXPECT_FALSE(p.isValid());
        EXPECT_EQ(p.nowMs(), 0LL);
    }

    TEST_F(NtpTimeProviderTest, ValidOnceNtpReportsSyncedReturnsFullEpochMs) {
        g_fake.synced = true;
        g_fake.epoch = 1'700'000'000;  // 2023-11-14 UTC
        g_fake.localTick = 12'345;

        NtpTimeProvider p(&fakeIsSynced, &fakeEpoch, &fakeLocalTick);
        EXPECT_TRUE(p.isValid());

        // Full 64-bit epoch-ms — no truncation. Value is way past uint32 range.
        const int64_t expected = static_cast<int64_t>(g_fake.epoch) * 1000LL;
        EXPECT_EQ(p.nowMs(), expected);
        EXPECT_GT(p.nowMs(), 0xFFFFFFFFLL);  // proves the wider type carries
    }

    TEST_F(NtpTimeProviderTest, EpochLeNtpSyncedFalseIsInvalid) {
        // Synced flag true but epoch fell back to 0 — treat as invalid so
        // TimeControl falls back to local millis().
        g_fake.synced = true;
        g_fake.epoch = 0;

        NtpTimeProvider p(&fakeIsSynced, &fakeEpoch, &fakeLocalTick);
        EXPECT_FALSE(p.isValid());
        EXPECT_EQ(p.nowMs(), 0LL);
    }

    // ---- Monotonic arithmetic between refreshes ----

    TEST_F(NtpTimeProviderTest, NowAdvancesWithLocalTickBetweenRefreshes) {
        g_fake.synced = true;
        g_fake.epoch = 1'700'000'000;
        g_fake.localTick = 1'000;

        NtpTimeProvider p(&fakeIsSynced, &fakeEpoch, &fakeLocalTick);
        const int64_t t0 = p.nowMs();

        // Advance only the local tick. Within the TTL, the provider must
        // produce a monotonically increasing value via pure arithmetic.
        g_fake.localTick = 1'500;
        const int64_t t1 = p.nowMs();
        EXPECT_EQ(t1 - t0, 500LL);

        g_fake.localTick = 2'250;
        const int64_t t2 = p.nowMs();
        EXPECT_EQ(t2 - t0, 1'250LL);
    }

    // ---- TTL / cache behaviour ----

    TEST_F(NtpTimeProviderTest, EpochReadOnceWithinTtl) {
        g_fake.synced = true;
        g_fake.epoch = 1'700'000'000;
        g_fake.localTick = 0;

        NtpTimeProvider p(&countingIsSynced, &countingEpoch, &countingLocalTick);
        p.setRefreshIntervalMs(10'000);  // 10 s TTL

        for (int i = 0; i < 500; ++i) {
            g_fake.localTick += 5;  // 500 × 5 ms = 2.5 s — inside the TTL
            (void)p.nowMs();
        }

        // 500 reads, 1 epoch fetch. Flagging >1 would mean the cache is
        // broken and we're hammering the backend.
        EXPECT_EQ(g_epochCalls, 1);
    }

    TEST_F(NtpTimeProviderTest, RefreshHappensAfterTtlExpires) {
        g_fake.synced = true;
        g_fake.epoch = 1'700'000'000;
        g_fake.localTick = 0;

        NtpTimeProvider p(&countingIsSynced, &countingEpoch, &countingLocalTick);
        p.setRefreshIntervalMs(1'000);  // 1 s TTL

        p.nowMs();                      // first read → 1 fetch
        EXPECT_EQ(g_epochCalls, 1);

        g_fake.localTick = 500;
        p.nowMs();                      // inside TTL → no new fetch
        EXPECT_EQ(g_epochCalls, 1);

        g_fake.localTick = 1'500;       // TTL has expired
        g_fake.epoch = 1'700'000'060;   // backend has drifted forward
        p.nowMs();
        EXPECT_EQ(g_epochCalls, 2);     // re-anchored
    }

    TEST_F(NtpTimeProviderTest, ZeroTtlDisablesCache) {
        g_fake.synced = true;
        g_fake.epoch = 1'700'000'000;

        NtpTimeProvider p(&countingIsSynced, &countingEpoch, &countingLocalTick);
        p.setRefreshIntervalMs(0);  // every call must refetch

        for (int i = 0; i < 5; ++i) {
            g_fake.localTick += 1;
            (void)p.nowMs();
        }
        EXPECT_EQ(g_epochCalls, 5);
    }

    TEST_F(NtpTimeProviderTest, RefreshAfterSyncLossRebindsOnNextValidSync) {
        g_fake.synced = true;
        g_fake.epoch = 1'700'000'000;
        g_fake.localTick = 0;

        NtpTimeProvider p(&fakeIsSynced, &fakeEpoch, &fakeLocalTick);
        p.setRefreshIntervalMs(100);
        EXPECT_TRUE(p.isValid());

        // Lose sync mid-run (radio drop, peer gone, etc.).
        g_fake.synced = false;
        g_fake.localTick = 200;  // force past TTL
        EXPECT_FALSE(p.isValid());
        EXPECT_EQ(p.nowMs(), 0LL);

        // Recover.
        g_fake.synced = true;
        g_fake.epoch = 1'700'001'000;
        g_fake.localTick = 400;
        EXPECT_TRUE(p.isValid());
        EXPECT_NE(p.nowMs(), 0LL);
    }

    // ---- Integration with TimeControl ----

    TEST_F(NtpTimeProviderTest, InstallsAsTimeControlProvider) {
        g_fake.synced = true;
        g_fake.epoch = 1'700'000'000;
        g_fake.localTick = 42;

        NtpTimeProvider p(&fakeIsSynced, &fakeEpoch, &fakeLocalTick);
        TimeControl::setTimeProvider(&p);

        EXPECT_EQ(TimeControl::now(), p.nowMs());
    }

    TEST_F(NtpTimeProviderTest, TimeControlFallsBackToLocalWhenProviderInvalid) {
        g_fake.synced = false;  // provider will be invalid

        NtpTimeProvider p(&fakeIsSynced, &fakeEpoch, &fakeLocalTick);
        TimeControl::setTimeProvider(&p);

        // Provider returns 0 on invalid, TimeControl must fall back to
        // local monotonic millis() — which is NOT zero (test has been
        // running for ms already).
        const int64_t t = TimeControl::now();
        // Strict: can't guarantee t > 0 on very first tick, but we can
        // assert it matches millis() exactly (fallback path).
        EXPECT_EQ(t, TimeControl::millis());
    }

}  // namespace
