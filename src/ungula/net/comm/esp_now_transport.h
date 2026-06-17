// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include "i_transport.h"

// -----------------------------------------------------------------------------
// EspNowTransport — ESP-NOW implementation of ITransport. This header names NO
// ESP type (the radio + C callbacks live entirely in the platform .cpp), so it
// stays host-includable. The implementation is selected at build time:
//   - esp32_esp_now_transport.cpp : ESP-IDF ESP-NOW (ESP_PLATFORM)
//   - mock_esp_now_transport.cpp  : host / test no-op (UNGULA_NET_MOCK)
// Agnostic code depends only on ITransport; the project builds the concrete
// transport at its composition root.
// -----------------------------------------------------------------------------

namespace ungula::net::comm
{

/// ESP-NOW transport implementation
class EspNowTransport : public ITransport {
    public:
        EspNowTransport();
        ~EspNowTransport() override;

        TransportError init() override;
        TransportError shutdown() override;
        TransportError send(const MacAddress &dst, const uint8_t *data, uint16_t len) override;
        void onReceive(TransportReceiveCallback callback) override;
        void onSendComplete(TransportSendCallback callback) override;
        const MacAddress &getOwnMac() const override;
        TransportError setChannel(uint8_t channel) override;
        uint8_t getChannel() const override;
        TransportError addPeer(const MacAddress &mac, uint8_t channel) override;
        TransportError removePeer(const MacAddress &mac) override;
        bool hasPeer(const MacAddress &mac) const override;

    private:
        bool initialized_;
        MacAddress ownMac_;
        // The ESP-NOW C receive/send callbacks live as file-static functions in
        // esp32_esp_now_transport.cpp — kept out of this header so it names no
        // ESP type and stays host-includable.
};

} // namespace ungula::net::comm
