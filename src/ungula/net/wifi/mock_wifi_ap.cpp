// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

// =============================================================================
// wifi_ap — mock / host stub. Selected by -DUNGULA_NET_MOCK. Reports "AP down"
// and does no real work, so host builds and tests link without an ESP WiFi
// stack. Wholly guarded so it never collides with a real platform impl.
// =============================================================================
#if defined(UNGULA_NET_MOCK)

#include "wifi_ap.h"

namespace ungula::net::wifi
{

bool ap_init(const WifiApConfig & /*config*/)
{
        return false;
}

bool wifi_stack_up_sta()
{
        return false;
}

const char *ap_get_ip()
{
        return "";
}

const char *ap_get_sta_ip()
{
        return "";
}

bool ap_sta_connected()
{
        return false;
}

const char *ap_get_mac()
{
        return "";
}

bool ap_is_active()
{
        return false;
}

WifiChannel ap_get_channel()
{
        return WifiChannel::ChAuto;
}

} // namespace ungula::net::wifi

#endif // UNGULA_NET_MOCK
