// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

// =============================================================================
// NTP client — mock / host stub. Selected by -DUNGULA_NET_MOCK (host tests and
// desktop builds opt in explicitly). Always reports "not synced" and does no
// real work, so the link seams resolve and the lib's tests can inject their own
// fakes into NtpTimeProvider. Wholly guarded so it never collides with a real
// platform implementation.
// =============================================================================
#if defined(UNGULA_NET_MOCK)

#include "ntp_client.h"

namespace ungula::net::ntp
{

void ntp_init(const NtpConfig & /*config*/)
{
}

void ntp_stop()
{
}

void resync()
{
}

bool ntp_is_synced()
{
        return false;
}

time_t ntp_epoch()
{
        return 0;
}

} // namespace ungula::net::ntp

#endif // UNGULA_NET_MOCK
