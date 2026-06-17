// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

// =============================================================================
// EspNowTransport — mock / host stub. Selected by -DUNGULA_NET_MOCK. Every
// method is a no-op (init/send report success but do nothing), so host builds
// and tests link without an ESP radio. Wholly guarded so it never collides with
// a real platform impl. Host code that wants real behaviour uses a different
// ITransport implementation.
// =============================================================================
#if defined(UNGULA_NET_MOCK)

#include "esp_now_transport.h"

namespace ungula::net::comm
{

EspNowTransport::EspNowTransport()
        : initialized_(false)
{
        ownMac_.clear();
}

EspNowTransport::~EspNowTransport()
{
}

TransportError EspNowTransport::init()
{
        initialized_ = true;
        return TransportError::Ok;
}

TransportError EspNowTransport::shutdown()
{
        initialized_ = false;
        return TransportError::Ok;
}

TransportError EspNowTransport::send(const MacAddress & /*dst*/, const uint8_t * /*data*/, uint16_t /*len*/)
{
        return TransportError::Ok;
}

void EspNowTransport::onReceive(TransportReceiveCallback /*callback*/)
{
}

void EspNowTransport::onSendComplete(TransportSendCallback /*callback*/)
{
}

const MacAddress &EspNowTransport::getOwnMac() const
{
        return ownMac_;
}

TransportError EspNowTransport::setChannel(uint8_t /*channel*/)
{
        return TransportError::Ok;
}

uint8_t EspNowTransport::getChannel() const
{
        return 0;
}

TransportError EspNowTransport::addPeer(const MacAddress & /*mac*/, uint8_t /*channel*/)
{
        return TransportError::Ok;
}

TransportError EspNowTransport::removePeer(const MacAddress & /*mac*/)
{
        return TransportError::Ok;
}

bool EspNowTransport::hasPeer(const MacAddress & /*mac*/) const
{
        return false;
}

} // namespace ungula::net::comm

#endif // UNGULA_NET_MOCK
