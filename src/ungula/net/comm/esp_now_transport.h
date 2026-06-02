// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

#pragma once

#include <esp_idf_version.h>
#include <esp_now.h>

#include "i_transport.h"

namespace ungula::net::comm
{

/// ESP-NOW transport implementation
class EspNowTransport : public ITransport {
    public:
        EspNowTransport();
        ~EspNowTransport() override;

        TransportError init() override;
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

        // ESP-NOW requires static C callbacks.
        static void onDataRecvCb(const esp_now_recv_info_t *info, const uint8_t *data, int len);
        // The send-cb's first parameter changed in IDF 5.4: a raw dest-MAC
        // pointer (<=5.3) became esp_now_send_info_t* (== wifi_tx_info_t*, the
        // dest MAC now lives in ->des_addr). Match the SDK so the function
        // pointer is assignable to esp_now_send_cb_t on both 5.1 and 5.5+.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
        static void onDataSentCb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
#else
        static void onDataSentCb(const uint8_t *mac, esp_now_send_status_t status);
#endif
};

} // namespace ungula::net::comm
