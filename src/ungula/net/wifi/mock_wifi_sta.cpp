// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

// =============================================================================
// wifi_sta — mock / host stub. Selected by -DUNGULA_NET_MOCK. Reports "STA not
// connected" and does no real work, so host builds and tests link without an
// ESP WiFi stack. Wholly guarded so it never collides with a real platform impl.
// =============================================================================
#if defined(UNGULA_NET_MOCK)

#include "wifi_sta.h"

namespace ungula::net::wifi
{

bool sta_init()
{
        return false;
}

bool sta_connect(const WifiStaConfig & /*config*/)
{
        return false;
}

void sta_disconnect()
{
}

bool sta_is_connected()
{
        return false;
}

const char *sta_get_ip()
{
        return "";
}

const char *sta_get_mac()
{
        return "";
}

WifiChannel sta_get_channel()
{
        return WifiChannel::ChAuto;
}

void sta_refresh_dns()
{
}

} // namespace ungula::net::wifi

#endif // UNGULA_NET_MOCK
