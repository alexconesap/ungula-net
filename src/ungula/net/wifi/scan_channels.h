// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

#pragma once

#include <cstdint>

// -------------------------------------------------------------------
// Default ESP-NOW pairing-discovery scan set: the non-overlapping 2.4 GHz
// channels. Most deployments use these as-is. A project that needs a different
// RF plan keeps its own array and ignores these — see the project's
// scan-channel header for the override point.
// -------------------------------------------------------------------

namespace ungula::net::wifi
{

inline constexpr uint8_t DEFAULT_SCAN_CHANNELS[] = { 1, 6, 11 };
inline constexpr uint8_t DEFAULT_SCAN_CHANNEL_COUNT =
        sizeof(DEFAULT_SCAN_CHANNELS) / sizeof(DEFAULT_SCAN_CHANNELS[0]);

} // namespace ungula::net::wifi
