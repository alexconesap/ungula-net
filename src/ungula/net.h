// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once
#ifndef __cplusplus
#error UngulaNet requires a C++ compiler
#endif

// Ungula Networking Library
// Communication, pairing, connection management, WiFi, HTTP

#include <ungula/core.h>

// Communication (transport interfaces, ESP-NOW, message header)
#include "ungula/net/comm/i_transport.h"
#include "ungula/net/comm/message_header.h"
#include "ungula/net/comm/transport_types.h"

// WiFi types
#include "ungula/net/wifi/wifi_channel.h"

// WiFi AP
#include "ungula/net/wifi/wifi_ap.h"

// WiFi STA (station mode — connect to routers, scan, ESP-NOW init)
#include "ungula/net/wifi/wifi_config.h"
#include "ungula/net/wifi/wifi_sta.h"

// Pairing types
#include "ungula/net/pairing/pairing_types.h"

// HTTP server and client
#include "ungula/net/http/http_client.h"
#include "ungula/net/http/http_server.h"

// NTP time synchronisation
#include "ungula/net/ntp/ntp_client.h"
#include "ungula/net/ntp/ntp_time_provider.h"
