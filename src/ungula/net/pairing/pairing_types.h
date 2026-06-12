// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <cstdint>

namespace ungula::net::pairing
{

/// Pairing state machine states
enum class PairingState : uint8_t {
        Idle = 0,
        Scanning = 1,
        Broadcasting = 2,
        Responding = 3,
        Confirming = 4,
        Paired = 5,
        Failed = 6
};

/// Pairing beacon magic bytes for identification
static constexpr uint8_t PAIRING_MAGIC[4] = { 0xD1, 0x5C, 0x0A, 0x11 };

/// Time to listen on each channel during scanning (ms)
static constexpr uint16_t CHANNEL_SCAN_DWELL_MS = 200;

/// Maximum number of WiFi channels to scan
static constexpr uint8_t MAX_SCAN_CHANNELS = 13;

/// Pairing beacon broadcast interval (ms)
static constexpr uint16_t BEACON_INTERVAL_MS = 100;

/// Pairing response timeout (ms)
static constexpr uint32_t PAIRING_TIMEOUT_MS = 10000;

/// Preference keys for persisted pairing data
static constexpr const char *PREF_KEY_PAIRED_MAC = "pair_mac";
static constexpr const char *PREF_KEY_PAIRED_CHANNEL = "pair_ch";
static constexpr const char *PREF_KEY_PAIRED_FLAG = "pair_ok";

/// Pairing role constants
static constexpr uint8_t PAIRING_ROLE_COORDINATOR = 1;
static constexpr uint8_t PAIRING_ROLE_CLIENT = 2;

/// Convert PairingState to string for logging
inline const char *pairingStateToString(PairingState state)
{
        switch (state) {
        case PairingState::Idle:
                return "IDLE";
        case PairingState::Scanning:
                return "SCANNING";
        case PairingState::Broadcasting:
                return "BROADCASTING";
        case PairingState::Responding:
                return "RESPONDING";
        case PairingState::Confirming:
                return "CONFIRMING";
        case PairingState::Paired:
                return "PAIRED";
        case PairingState::Failed:
                return "FAILED";
        default:
                return "UNKNOWN";
        }
}

} // namespace ungula::net::pairing
