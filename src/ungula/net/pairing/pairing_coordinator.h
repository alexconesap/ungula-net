// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once
#include <ungula/core/preferences/i_preferences.h>
#include <ungula/net/comm/i_transport.h>
#include <ungula/net/connection/reconnect_messages.h>
#include "pairing_beacon.h"
#include "pairing_types.h"

namespace ungula::net::pairing
{

/// Maximum number of paired clients
static constexpr uint8_t MAX_PAIRED_CLIENTS = 2;

/// Info about one paired client
struct PairedClientInfo {
        comm::MacAddress mac;
        uint8_t deviceId;
        bool active;

        PairedClientInfo()
                : deviceId(0)
                , active(false)
        {
                mac.clear();
        }
};

/// Event data for a newly paired client
struct PairedClientEvent {
        comm::MacAddress mac;
        uint8_t deviceId;
};

/// Callback when a new client pairs
using OnClientPairedCallback = void (*)(const PairedClientEvent &event);

/// Coordinator-side pairing manager
class PairingCoordinator {
    public:
        /// @param transport   Transport layer
        /// @param prefs       Preferences for persistence
        /// @param prefsNs     Namespace for pairing data
        PairingCoordinator(ungula::net::comm::ITransport &transport,
                           ungula::core::preferences::IPreferences &prefs, const char *prefsNs);

        /// Load previously paired clients from preferences
        void loadPairedClients();

        /// Enable pairing mode (start broadcasting beacons)
        void enablePairing();

        /// Disable pairing mode
        void disablePairing();

        /// Check if pairing mode is active
        bool isPairingEnabled() const;

        /// Must be called from the main loop
        void loop(uint32_t nowMs);

        /// Handle a received message
        /// @return true if consumed as a pairing message
        bool handleReceived(const ungula::net::comm::MacAddress &srcMac, const uint8_t *data,
                            uint16_t len);

        /// Register callback for when a client pairs
        void onClientPaired(OnClientPairedCallback cb);

        /// Get paired client info by index
        const PairedClientInfo *getPairedClient(uint8_t index) const;

        /// Check if a MAC is paired
        bool isPaired(const ungula::net::comm::MacAddress &mac) const;

        /// Get number of active paired clients
        uint8_t pairedClientCount() const;

        /// Unpair all clients
        void unpairAll();

    private:
        ungula::net::comm::ITransport &transport_;
        ungula::core::preferences::IPreferences &prefs_;
        const char *prefsNs_;

        bool pairingEnabled_;
        uint32_t lastBeaconMs_;
        PairedClientInfo clients_[MAX_PAIRED_CLIENTS];
        OnClientPairedCallback onPairedCb_;

        void broadcastBeacon();
        void handlePairingRequest(const ungula::net::comm::MacAddress &srcMac,
                                  const PairingRequest &req);
        void handleReconnectProbe(const ungula::net::comm::MacAddress &srcMac,
                                  const ungula::net::connection::ReconnectProbe &probe);
        bool storePairedClient(const ungula::net::comm::MacAddress &mac, uint8_t deviceId);
};

} // namespace ungula::net::pairing
