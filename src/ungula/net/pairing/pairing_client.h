// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <ungula/core/preferences/i_preferences.h>
#include <ungula/net/comm/i_transport.h>
#include <ungula/net/comm/transport_types.h>
#include "pairing_beacon.h"
#include "pairing_types.h"

namespace ungula::net::pairing
{

/// Result of checking for existing pairing
struct StoredPairing {
        ungula::net::comm::MacAddress coordinatorMac;
        uint8_t channel;
        bool valid;

        StoredPairing()
                : channel(0)
                , valid(false)
        {
                coordinatorMac.clear();
        }
};

/// Callback when pairing completes
using OnPairedCallback = void (*)(const ungula::net::comm::MacAddress &mac, uint8_t channel);

/// Client-side pairing manager
class PairingClient {
    public:
        /// @param transport   Transport layer
        /// @param prefs       Preferences for persistence
        /// @param prefsNs     Namespace
        /// @param deviceId    This device's application-level ID
        PairingClient(ungula::net::comm::ITransport &transport,
                      ungula::core::preferences::IPreferences &prefs, const char *prefsNs,
                      uint8_t deviceId);

        /// Load stored pairing from preferences
        StoredPairing loadStoredPairing();

        /// Set the list of channels to scan (must outlive this object).
        /// If not called, defaults to scanning channels 1..13.
        void setScanChannels(const uint8_t *channels, uint8_t count);

        /// Start scanning for coordinator beacon
        void startScanning();

        /// Stop scanning
        void stopScanning();

        /// Check if currently scanning
        bool isScanning() const;

        /// Check if paired
        bool isPaired() const;

        /// Get the pairing state
        PairingState getState() const;

        /// Must be called from the main loop
        void loop(uint32_t nowMs);

        /// Handle a received message
        /// @param nowMs Current time in milliseconds (used for FSM timeouts)
        /// @return true if consumed as a pairing message
        bool handleReceived(const ungula::net::comm::MacAddress &srcMac, const uint8_t *data,
                            uint16_t len, uint32_t nowMs);

        /// Register callback for pairing completion
        void onPaired(OnPairedCallback cb);

        /// Get the coordinator MAC (valid only when paired)
        const ungula::net::comm::MacAddress &getCoordinatorMac() const;

        /// Get the paired channel (valid only when paired)
        uint8_t getPairedChannel() const;

        /// Update the paired channel and persist to NVS
        void setPairedChannel(uint8_t channel);

        /// Get this device's application-level ID
        uint8_t getDeviceId() const
        {
                return deviceId_;
        }

        /// Get the scan channel list and count
        const uint8_t *getScanChannels() const
        {
                return scanChannels_;
        }
        uint8_t getScanChannelCount() const
        {
                return scanChannelCount_;
        }

        /// Clear stored pairing
        void clearPairing();

    private:
        ungula::net::comm::ITransport &transport_;
        ungula::core::preferences::IPreferences &prefs_;
        const char *prefsNs_;
        uint8_t deviceId_;

        PairingState state_;
        const uint8_t *scanChannels_; // pointer to channel list (caller-owned)
        uint8_t scanChannelCount_; // number of entries in scanChannels_
        uint8_t scanIndex_; // current index into scanChannels_
        uint8_t currentScanChannel_;
        uint32_t channelStartMs_;
        uint32_t pairingStartMs_;
        ungula::net::comm::MacAddress coordinatorMac_;
        uint8_t pairedChannel_;
        OnPairedCallback onPairedCb_;

        void advanceChannel(uint32_t nowMs);
        void sendPairingRequest(const ungula::net::comm::MacAddress &coordinatorMac);
        void handleBeacon(const ungula::net::comm::MacAddress &srcMac, const PairingBeacon &beacon,
                          uint32_t nowMs);
        void handleConfirm(const ungula::net::comm::MacAddress &srcMac,
                           const PairingConfirm &confirm);
        void storePairing();
};

} // namespace ungula::net::pairing
