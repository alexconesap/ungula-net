# UngulaNet (`lib_net`)

ESP32-only networking stack on top of ESP-IDF: WiFi (AP / STA / AP+STA),
ESP-NOW transport with multi-channel pairing, a transport-agnostic
connection manager, an HTTP+WebSocket server on a single port, an HTTP/S
client, and an SNTP-based time provider that plugs into UngulaCore's
`ungula::core::time`. No Arduino dependencies.

Depends on `UngulaCore` and `EmblogX`. Targets ESP32 only.

Every platform-specific `.cpp` is wholly wrapped in a build guard, so
exactly one implementation of each seam links. Define what you need:

| Define | Selects |
| ------ | ------- |
| `ESP_PLATFORM` | ESP-IDF implementations — ESP-NOW, WiFi AP/STA, httpd, `esp_http_client`, SNTP. |
| `UNGULA_NET_MOCK` | Host stubs — ESP-NOW, WiFi AP/STA, HTTP server, NTP all no-op and report "down"/"not synced". Lets host tests link. |
| `UNGULA_NET_CURL` | Real libcurl HTTP client for desktop. Independent of `UNGULA_NET_MOCK`. |
| `CONFIG_HTTPD_STACK` | httpd task stack in bytes, default 12288. |
| `WIFI_NVS_NAMESPACE_VALUE` | Default NVS namespace for `WifiConfigStore`, default `"main_wifi"`. |

With no define set, the platform seams have no definition and the link
fails — that is deliberate, not a silent fallback.

The mock STA stub is partial: `sta_scan`, `sta_get_cached_dns_main` and
`sta_get_cached_dns_backup` have no host implementation, so host code
that calls them will not link.

Umbrella header: `<ungula/net.h>` — pulls in `comm/i_transport.h`,
`comm/message_header.h`, `comm/transport_types.h`, `wifi/wifi_channel.h`,
`wifi/wifi_ap.h`, `wifi/wifi_sta.h`, `wifi/wifi_config.h`,
`pairing/pairing_types.h`, `http/http_client.h`, `http/http_server.h`,
`ntp/ntp_client.h` and `ntp/ntp_time_provider.h`.

NOT in the umbrella — include these directly when you need them:
`comm/esp_now_transport.h`, `wifi/wifi_espnow.h`, `wifi/scan_channels.h`,
`pairing/pairing_beacon.h`, `pairing/pairing_client.h`,
`pairing/pairing_coordinator.h`, and everything under
`ungula/net/connection/`.

---

## LLM quick map

- **Primary include**: `#include <ungula/net.h>`.
- **Arduino discovery include**: `#include <ungula_net.h>` (forwarder only; host code should keep using the real header).
- **Namespace root**: `ungula::net`.
- **Own source minimum**: `C++20`.
- **Effective minimum for consumers**: `C++20`.
- **Dependency impact**: Own source uses designated initializers in `lib_net/src`, which requires `C++20`.
- **Supported architectures**: `esp32`.

### Use-case index

- [Use case: ESP-NOW only (no AP, no web server)](#use-case-esp-now-only-no-ap-no-web-server)
- [Use case: AP + REST + WebSocket portal](#use-case-ap-rest-websocket-portal)
- [Use case: HTTP / HTTPS client (push to cloud)](#use-case-http-https-client-push-to-cloud)
- [Use case: NTP-backed wall clock plugged into `ungula::core::time`](#use-case-ntp-backed-wall-clock-plugged-into-ungulacoretime)
- [Use case: Coordinator-side pairing (accept new clients)](#use-case-coordinator-side-pairing-accept-new-clients)
- [Use case: Client-side pairing (find a coordinator)](#use-case-client-side-pairing-find-a-coordinator)
- [Use case: Connection lifecycle with reacquisition (ESP-NOW)](#use-case-connection-lifecycle-with-reacquisition-esp-now)

Rules for using this API are collected once at the end of the file — see
[LLM usage rules](#llm-usage-rules).

## Usage

### Use case: ESP-NOW only (no AP, no web server)

> **MANDATORY on ESP-IDF**: call `ungula::core::preferences::initStorage()`
> (from `lib`) BEFORE `wifi::espnow_init()`. The WiFi driver persists
> calibration through NVS and fails with `ESP_ERR_NVS_NOT_INITIALIZED`
> (boot loop) otherwise. Arduino-ESP32 did this implicitly at startup —
> pure ESP-IDF does not.

```cpp
#include <ungula/core/preferences/preferences.h>  // initStorage()
#include <ungula/net.h>
#include <ungula/net/comm/esp_now_transport.h>
#include <ungula/net/wifi/wifi_espnow.h>          // espnow_init()

using namespace ungula::net::comm;

EspNowTransport transport;

static void onMessage(const MacAddress& src, const uint8_t* data, uint16_t len) {
    const MessageHeader* hdr = extractHeader(data, len);
    if (hdr == nullptr) return;
    // hdr->messageType, extractPayload(data, len), payloadLength(len)
}

void setup() {
    ungula::core::preferences::initStorage();  // REQUIRED on ESP-IDF — must come first
    ungula::net::wifi::espnow_init();          // WiFi STA, minimum required by ESP-NOW
    transport.init();
    transport.setChannel(6);
    transport.onReceive(onMessage);
}

void loop() {
    uint8_t buf[sizeof(MessageHeader)] = {};
    auto* hdr = reinterpret_cast<MessageHeader*>(buf);
    hdr->protocolVersion = 1;
    hdr->messageType     = 0x01;
    transport.send(MacAddress::broadcast(), buf, sizeof(buf));
    ungula::core::time::delayMs(1000);
}
```

When to use this: headless nodes that talk only to peers — no browser UI,
no internet.

### Use case: AP + REST + WebSocket portal

```cpp
#include <ungula/net/wifi/wifi_ap.h>
#include <ungula/net/http/http_server.h>

using ungula::net::http::HttpServer;
using ungula::net::http::HttpRequest;
using ungula::net::http::Method;

static HttpServer server;

static void handleStatus(HttpRequest& req) {
    req.sendJson(200, R"({"status":"ok"})");
}

void setup() {
    ungula::net::wifi::WifiApConfig cfg;
    cfg.ssid           = "MyDevice";
    cfg.password       = "secret123";
    cfg.channel        = ungula::net::wifi::WifiChannel::Ch6;
    cfg.maxConnections = 4;
    ungula::net::wifi::ap_init(cfg);

    server.start(80);
    server.route(Method::GET, "/api/status", handleStatus);
    server.enableWebSocket("/ws");
    server.ready();                       // call AFTER all routes
}

void loop() {
    static uint32_t last = 0;
    const auto now = static_cast<uint32_t>(ungula::core::time::millis());
    if (now - last >= 1000) {
        last = now;
        const char* json = R"({"uptime":1})";
        server.wsBroadcast(json, std::strlen(json));
    }
}
```

When to use this: device with a captive web portal that pushes live
state to one or more browser tabs.

### Use case: HTTP / HTTPS client (push to cloud)

```cpp
#include <ungula/net/http/http_client.h>

namespace http = ungula::net::http;

void pushStatus() {
    const char* body = R"({"device":"node-1","temp":350})";
    http::HttpResult r = http::httpPost(
        "https://api.example.com/status", body, std::strlen(body), 5000);
    if (r.success) {
        // r.statusCode, r.body, r.bodyLen, r.bodyContains("ok")
    }
}
```

When to use this: telemetry, health checks, fetching small JSON
configurations. Body is truncated to 1024 bytes.

### Use case: NTP-backed wall clock plugged into `ungula::core::time`

```cpp
#include <ungula/net/wifi/wifi_sta.h>
#include <ungula/net/ntp/ntp_client.h>
#include <ungula/net/ntp/ntp_time_provider.h>
#include <ungula/core/time/time.h>

void setup() {
    ungula::net::wifi::sta_init();
    ungula::net::wifi::WifiStaConfig sta;
    sta.ssid = "MyRouter"; sta.password = "...";
    ungula::net::wifi::sta_connect(sta);

    // One call does SNTP bring-up + provider install + logger clock.
    // Idempotent: call it again on every STA reconnect.
    ungula::net::ntp::ensure_system_clock();
    ungula::core::time::setTimezone(ungula::core::time::tz::Timezone::CET);
}

void log() {
    char ts[24];
    ungula::core::time::formatLocal(ts, sizeof(ts));   // "" until synced
}
```

When to use this: any time `ungula::core::time::now()` must return real
UTC-epoch-ms instead of monotonic-since-boot.

If you want a different time source (RTC, manual `setTime`), skip
`ensure_system_clock()` and wire it by hand instead:

```cpp
ungula::net::ntp::ntp_init();                       // SNTP up
static ungula::net::ntp::NtpTimeProvider clock;     // program lifetime
ungula::core::time::setTimeProvider(&clock);
```

### Use case: Coordinator-side pairing (accept new clients)

```cpp
#include <ungula/net/comm/esp_now_transport.h>
#include <ungula/net/pairing/pairing_coordinator.h>
#include <ungula/net/wifi/wifi_espnow.h>
#include <ungula/core/preferences/preferences.h>

using namespace ungula::net;

static comm::EspNowTransport transport;
static ungula::core::preferences::Preferences prefs;
static pairing::PairingCoordinator pair(transport, prefs, "pair_ns");

static void onRx(const comm::MacAddress& src, const uint8_t* data, uint16_t len) {
    if (pair.handleReceived(src, data, len)) return;
    // ... application messages
}

static void onPaired(const pairing::PairedClientEvent& ev) {
    // ev.mac, ev.deviceId
}

void setup() {
    ungula::core::preferences::initStorage();  // REQUIRED on ESP-IDF — must come first
    wifi::espnow_init();
    transport.init();
    transport.setChannel(6);
    transport.onReceive(onRx);

    pair.loadPairedClients();
    pair.onClientPaired(&onPaired);
}

void onPairButton() { pair.enablePairing(); }

void loop() { pair.loop(static_cast<uint32_t>(ungula::core::time::millis())); }
```

`handleReceived` runs whatever the transport hands it, so it inherits the
transport's context — see [Threading](#threading--timing--hardware-notes)
before calling it straight from the ESP-NOW receive callback.

### Use case: Client-side pairing (find a coordinator)

```cpp
#include <ungula/net/comm/esp_now_transport.h>
#include <ungula/net/pairing/pairing_client.h>
#include <ungula/net/wifi/scan_channels.h>
#include <ungula/net/wifi/wifi_espnow.h>
#include <ungula/core/preferences/preferences.h>

using namespace ungula::net;

static comm::EspNowTransport transport;
static ungula::core::preferences::Preferences prefs;
static pairing::PairingClient pair(transport, prefs, "pair_ns", /*deviceId=*/7);

static void onPaired(const comm::MacAddress& mac, uint8_t channel) {
    // pair.getCoordinatorMac(), pair.getPairedChannel()
}

void setup() {
    ungula::core::preferences::initStorage();  // REQUIRED on ESP-IDF — must come first
    wifi::espnow_init();
    transport.init();

    // Static storage — setScanChannels keeps the pointer, it does not copy.
    pair.setScanChannels(wifi::DEFAULT_SCAN_CHANNELS,
                         wifi::DEFAULT_SCAN_CHANNEL_COUNT);   // {1, 6, 11}
    pair.onPaired(&onPaired);

    // setScanChannels FIRST — loadStoredPairing validates the stored
    // channel against the scan list and falls back to scanChannels[0].
    auto stored = pair.loadStoredPairing();
    if (!stored.valid) pair.startScanning();
}

void loop() {
    pair.loop(static_cast<uint32_t>(ungula::core::time::millis()));
}
```

### Use case: Connection lifecycle with reacquisition (ESP-NOW)

```cpp
#include <ungula/net/comm/esp_now_transport.h>
#include <ungula/net/connection/connection_manager.h>
#include <ungula/net/connection/espnow_session_provider.h>
#include <ungula/net/pairing/pairing_client.h>

using namespace ungula::net;

static comm::EspNowTransport  transport;
static ungula::core::preferences::Preferences prefs;
static pairing::PairingClient pair(transport, prefs, "pair_ns", 7);

static void sendHeartbeat(const comm::MacAddress& coord, void* /*ctx*/) {
    // build + transport.send(coord, ...);
}

static connection::EspNowSessionProvider session(transport, pair, &sendHeartbeat, nullptr);
static connection::ConnectionConfig  cfg;     // defaults are reasonable
static connection::ConnectionManager conn(session, cfg);

void setup() {
    conn.begin(static_cast<uint32_t>(ungula::core::time::millis()));
}

// Called from your receive path for every packet the coordinator sent.
void onAnyMessageFromCoordinator(const uint8_t* data, uint16_t len) {
    const auto now = static_cast<uint32_t>(ungula::core::time::millis());

    // Reacquisition acks come back as a ReconnectAck. Check the length
    // yourself — onReconnectAck takes an already-typed reference.
    if (len >= sizeof(connection::ReconnectAck)) {
        const auto* ack = reinterpret_cast<const connection::ReconnectAck*>(data);
        if (ack->isValid() && session.onReconnectAck(*ack)) {
            conn.onReacquisitionSuccess(now);
            return;
        }
    }
    conn.onMessageReceived(now);
}

void loop() {
    conn.loop(static_cast<uint32_t>(ungula::core::time::millis()));
}
```

When to use this: a node that must auto-recover after coordinator
reboots, channel changes, or transient ESP-NOW drops.

---

## Public types

| Type | Header | Purpose |
| ---- | ------ | ------- |
| `comm::ITransport` | `ungula/net/comm/i_transport.h` | Abstract transport |
| `comm::EspNowTransport` | `ungula/net/comm/esp_now_transport.h` | ESP-NOW backend |
| `comm::MacAddress` (POD) | `ungula/net/comm/transport_types.h` | 6-byte MAC value |
| `comm::TransportError` (enum) | `ungula/net/comm/transport_types.h` | Ok/SendFailed/… |
| `comm::MessageHeader` (packed, 8B) | `ungula/net/comm/message_header.h` | Wire header |
| `wifi::WifiChannel` (enum) | `ungula/net/wifi/wifi_channel.h` | Ch1..Ch13, ChAuto |
| `wifi::DEFAULT_SCAN_CHANNELS` | `ungula/net/wifi/scan_channels.h` | `{1, 6, 11}` default pairing scan set |
| `wifi::WifiApConfig` | `ungula/net/wifi/wifi_ap.h` | AP setup |
| `wifi::WifiStaConfig`, `WifiScanResult` | `ungula/net/wifi/wifi_sta.h` | STA + scanning |
| `wifi::WifiConfig`, `WifiConfigStore` | `ungula/net/wifi/wifi_config.h` | NVS-backed STA creds |
| `pairing::PairingCoordinator` | `ungula/net/pairing/pairing_coordinator.h` | Coordinator FSM |
| `pairing::PairingClient` | `ungula/net/pairing/pairing_client.h` | Client FSM |
| `pairing::PairedClientInfo`, `PairedClientEvent` | `ungula/net/pairing/pairing_coordinator.h` | Coordinator state |
| `pairing::StoredPairing` | `ungula/net/pairing/pairing_client.h` | Cached pairing |
| `pairing::PairingState` (enum) | `ungula/net/pairing/pairing_types.h` | Pairing FSM |
| `pairing::PairingBeacon/Request/Confirm` (packed, 8B) | `ungula/net/pairing/pairing_beacon.h` | Wire structs |
| `connection::ConnectionManager` | `ungula/net/connection/connection_manager.h` | Connection FSM |
| `connection::ConnectionConfig`, `ConnectionPolicy` | `ungula/net/connection/connection_config.h` | Tuning |
| `connection::ConnMgrState` (enum) | `ungula/net/connection/connection_config.h` | FSM state |
| `connection::ISessionProvider` | `ungula/net/connection/i_session_provider.h` | Transport adapter |
| `connection::EspNowSessionProvider` | `ungula/net/connection/espnow_session_provider.h` | ESP-NOW adapter |
| `connection::ReconnectProbe`, `ReconnectAck` (packed, 8B) | `ungula/net/connection/reconnect_messages.h` | Reacquisition wire |
| `http::HttpServer`, `HttpRequest` | `ungula/net/http/http_server.h` | HTTP+WS server |
| `http::Method`, `RouteHandler` | `ungula/net/http/http_server.h` | Routes |
| `http::HttpResult` | `ungula/net/http/http_client.h` | Client response |
| `ntp::NtpConfig` | `ungula/net/ntp/ntp_client.h` | NTP setup |
| `ntp::NtpTimeProvider` | `ungula/net/ntp/ntp_time_provider.h` | `ITimeProvider` adapter |

Two free helpers turn the FSM enums into strings for logging:
`connection::connMgrStateToString(ConnMgrState)` and
`pairing::pairingStateToString(PairingState)`.

Constants worth knowing:

- `comm::TRANSPORT_MAX_PAYLOAD = 240` — single ESP-NOW message cap.
- `pairing::MAX_PAIRED_CLIENTS = 2` — coordinator slot count.
- `pairing::MAX_SCAN_CHANNELS = 13`, `BEACON_INTERVAL_MS = 100`,
  `CHANNEL_SCAN_DWELL_MS = 200`, `PAIRING_TIMEOUT_MS = 10000`.
- `http::MAX_ROUTES = 40`, `HttpServer::MAX_WS_CLIENTS = 4`,
  `HttpRequest::MAX_PARAMS = 32` (name 32 B, value 48 B each), request
  body buffer 768 bytes, URI buffer 96 bytes, `HttpResult::body` 1024
  bytes (truncated silently above this).
- `wifi::WIFI_MAX_SCAN_RESULTS = 16`,
  `WIFI_SSID_MAX_LEN = 33`, `WIFI_PASS_MAX_LEN = 65`.
- `wifi::DEFAULT_SCAN_CHANNELS = {1, 6, 11}`,
  `DEFAULT_SCAN_CHANNEL_COUNT = 3`.

An `HttpRequest` is ~3.4 KB and lives on the httpd task stack for the
duration of a request. Budget for it when sizing `CONFIG_HTTPD_STACK`.

---

## Public functions / methods

### `comm::ITransport`

| Member | Notes |
| ------ | ----- |
| `TransportError init()` | Bring up the radio. |
| `TransportError shutdown()` | Release the link layer and hand the radio back (e.g. so the STA can take it for an OTA download). Default is a no-op; `EspNowTransport` calls `esp_now_deinit()`. Re-`init()` to come back. |
| `TransportError send(const MacAddress&, const uint8_t* data, uint16_t len)` | `len ≤ TRANSPORT_MAX_PAYLOAD`. Unicast requires `addPeer` first. Broadcast uses `MacAddress::broadcast()` and the broadcast address must be added as a peer too. |
| `void onReceive(TransportReceiveCallback)` | C function pointer. Buffer valid only during the call. |
| `void onSendComplete(TransportSendCallback)` | Signals ACK / failure. |
| `const MacAddress& getOwnMac()` |  |
| `TransportError setChannel(uint8_t 1..13)` / `uint8_t getChannel()` |  |
| `TransportError addPeer(const MacAddress&, uint8_t channel = 0)` / `removePeer` / `bool hasPeer` |  |

`EspNowTransport` is the only concrete implementation; instantiate it directly.

### `comm::MacAddress`

POD. `static MacAddress fromBytes(const uint8_t*)`,
`static constexpr MacAddress broadcast()`, `bool isZero()`,
`bool isBroadcast()`, `void clear()`, `void copyFrom(const uint8_t*)`,
`operator==/!=`. Members: `addr[6]`, `static constexpr ADDR_LEN = 6`.

### `comm::MessageHeader` helpers (`ungula/net/comm/message_header.h`)

- `bool isValidHeader(const uint8_t* data, uint16_t len, uint8_t expectedVersion)`
- `const MessageHeader* extractHeader(const uint8_t* data, uint16_t len)` — returns `nullptr` on short buffer.
- `const uint8_t* extractPayload(const uint8_t* data, uint16_t len)` — returns `nullptr` if no payload.
- `uint16_t payloadLength(uint16_t totalLen)`

`extractHeader` / `extractPayload` return pointers into the caller's
buffer and are the only length-checked entry points in the library — the
pairing and reconnect wire structs are cast directly, so validate the
length before handing raw frames to anything that takes a typed reference.

### `wifi::ap_*` (require ESP_PLATFORM)

`bool ap_init(const WifiApConfig&)`, `bool wifi_stack_up_sta()`,
`const char* ap_get_ip()`, `const char* ap_get_sta_ip()`,
`bool ap_sta_connected()`, `const char* ap_get_mac()`,
`bool ap_is_active()`, `WifiChannel ap_get_channel()`.

`WifiApConfig{ssid, password, channel, maxConnections}` defaults to
`ChAuto` and 4 connections. **`ssid` must be set** — it defaults to
`nullptr` and `ap_init` does not check it. An empty `password` gives an
open AP; anything else gives WPA2-PSK.

`wifi_stack_up_sta()` brings the stack up in STA mode, started but never
connected — the minimum PHY for ESP-NOW when no SoftAP is wanted. It
overlaps with `sta_init()`; pick one per firmware and stay with it.

### `wifi::sta_*` (require ESP_PLATFORM)

`bool sta_init()`, `bool sta_connect(const WifiStaConfig&)` (blocking,
default 15 s timeout), `void sta_disconnect()`, `bool sta_is_connected()`,
`const char* sta_get_ip()`, `const char* sta_get_mac()`,
`WifiChannel sta_get_channel()`, `void sta_refresh_dns()`,
`uint32_t sta_get_cached_dns_main()`, `uint32_t sta_get_cached_dns_backup()`,
`uint8_t sta_scan(WifiScanResult*, uint8_t maxResults,
const char* const* prefixes = nullptr, uint8_t prefixCount = 0)`.

`bool wifi::espnow_init()` — STA-only minimum for ESP-NOW.
**Requires `ungula::core::preferences::initStorage()` to have been called
first on ESP-IDF** (NVS prereq for the WiFi driver). Skipping it produces
`ESP_ERR_NVS_NOT_INITIALIZED` and a reboot loop.

### `wifi::WifiConfigStore`

`WifiConfig load()` — returns defaults on missing/CRC-mismatch.
`void save(const WifiConfig&)`, `void clear()`. CRC32 over the blob in
the NVS namespace given to the constructor.

**Conditional define `WIFI_NVS_NAMESPACE_VALUE`** — the constructor's
`nvsNamespace` parameter defaults to this build define, which itself defaults
to `"main_wifi"`. So a host can write `WifiConfigStore(prefs)` directly and
never wrap it. Override per project from the build (`.settings`), e.g.
`-DWIFI_NVS_NAMESPACE_VALUE='"icb_wifi"'`. The namespace only scopes the
credential blob inside each device's own NVS, so sharing the default across
projects is harmless.

### `pairing::PairingCoordinator`

| Member | Notes |
| ------ | ----- |
| `PairingCoordinator(comm::ITransport&, IPreferences&, const char* prefsNs)` | All references must outlive the object. |
| `void loadPairedClients()` | Restore from NVS. Call once at boot. |
| `void enablePairing() / disablePairing()` | Toggle beacon broadcasting. |
| `bool isPairingEnabled()` |  |
| `void loop(uint32_t nowMs)` | Drive the FSM (call every loop). |
| `bool handleReceived(const comm::MacAddress& src, const uint8_t* data, uint16_t len)` | `true` ⇒ consumed by pairing — do not process as application data. |
| `void onClientPaired(OnClientPairedCallback)` | `void(*)(const PairedClientEvent&)`. |
| `const PairedClientInfo* getPairedClient(uint8_t index)` |  |
| `bool isPaired(const comm::MacAddress&)`, `uint8_t pairedClientCount()`, `void unpairAll()` |  |
| `bool unpairClient(uint8_t deviceId)` | Drop one client (peer table + NVS) so its slot can re-pair. `true` if a slot matched. |

`handleReceived` also answers `ReconnectProbe` frames, and it does so
whether or not pairing mode is enabled — that is how a client that lost
the channel finds the coordinator again. Probes from a MAC that is not in
the paired table are dropped.

### `pairing::PairingClient`

| Member | Notes |
| ------ | ----- |
| `PairingClient(comm::ITransport&, IPreferences&, const char* prefsNs, uint8_t deviceId)` |  |
| `StoredPairing loadStoredPairing()` | Reads NVS, applies the channel to the transport and adds the coordinator as a peer. Call `setScanChannels` FIRST — a stored channel outside the scan list is replaced by `scanChannels[0]`. Sets the state to `Paired` when the MAC is good. |
| `void setScanChannels(const uint8_t* channels, uint8_t count)` | **Pointer-only** — array must outlive the client. |
| `void startScanning() / stopScanning()` |  |
| `bool isScanning() / isPaired()` |  |
| `PairingState getState()` |  |
| `void loop(uint32_t nowMs)` |  |
| `bool handleReceived(const comm::MacAddress& src, const uint8_t* data, uint16_t len, uint32_t nowMs)` | `true` ⇒ consumed. |
| `void onPaired(OnPairedCallback)` | `void(*)(const MacAddress&, uint8_t channel)`. |
| `const comm::MacAddress& getCoordinatorMac()`, `uint8_t getPairedChannel()`, `void setPairedChannel(uint8_t)` | Only valid when paired. |
| `uint8_t getDeviceId()`, `const uint8_t* getScanChannels()`, `uint8_t getScanChannelCount()`, `void clearPairing()` |  |

### `connection::ConnectionManager` + `ISessionProvider`

| Member | Notes |
| ------ | ----- |
| `ConnectionManager(ISessionProvider&, const ConnectionConfig&)` |  |
| `void begin(uint32_t nowMs)` | Start FSM after the session provider is ready. |
| `void loop(uint32_t nowMs)` | Drives state transitions. |
| `void onHeartbeatReceived(uint32_t nowMs)` / `onMessageReceived(uint32_t nowMs)` | Refresh "last heard" on any inbound traffic. |
| `void onReacquisitionSuccess(uint32_t nowMs)` | Called by the session provider when a probe ack arrives. |
| `bool isConnected()`, `ConnMgrState getState()` |  |

`ConnectionConfig` defaults: DYNAMIC policy, 2 s heartbeat timeout, 500 ms
degraded grace, 1 s static probe interval, 5 static probes before
escalating, 500 ms dynamic probe interval, 3 s boot grace.

`ConnectionPolicy::Static` never escalates to broad reacquisition — it
keeps probing the last known context forever, and `staticMaxProbes` is
ignored. Only `Dynamic` (the default) escalates.

`EspNowSessionProvider` constructor:
`(comm::ITransport&, pairing::PairingClient&, ProbeCallback, void* ctx)`
where `ProbeCallback = void(*)(const comm::MacAddress&, void*)`. Also
exposes `bool onReconnectAck(const ReconnectAck&)` to wire into the
receive path.

`onReconnectAck` takes an already-typed reference: **check
`len >= sizeof(ReconnectAck)` and `isValid()` yourself** before casting a
raw frame to it. It switches the transport channel and writes the new
channel to NVS, so it must not run in the ESP-NOW receive callback — see
[Threading](#threading--timing--hardware-notes). It returns `true` when
it consumed the ack; feed that into
`ConnectionManager::onReacquisitionSuccess`.

### `http::HttpServer` (require ESP_PLATFORM)

| Member | Notes |
| ------ | ----- |
| `bool start(uint16_t port = 80)` |  |
| `void stop()` |  |
| `void route(Method, const char* path, RouteHandler)` | `path` must be a string-literal-like address (stored by pointer). Up to `MAX_ROUTES`. |
| `void setNotFoundHandler(RouteHandler)` |  |
| `void ready()` | **Call once after all routes and `enableWebSocket`.** Registers wildcard dispatchers. |
| `bool enableWebSocket(const char* path = "/ws")` |  |
| `int wsBroadcast(const char* data, size_t len)` | Returns clients sent to. Server is broadcast-only — incoming frames ignored by design. |
| `int wsPing()` | Drops stale clients on PING failure. |
| `int wsClientCount()`, `bool isRunning()` |  |

`HttpRequest` (passed by reference to handlers):
`void send(int code, const char* contentType, const char* body)`,
`void sendProgmem(int code, const char* contentType, const char* progmem)`,
`void sendBinary(int code, const char* contentType, const uint8_t* data,
size_t len, const char* filename = nullptr)`,
`void sendJson(int code, const char* json)`,
`bool hasParam(const char* name)`, `const char* param(const char* name)`
(empty string when missing), `const char* uri()`, `const char* body()`
(POST/PUT body, capped at 768 bytes).

`param` covers the query string AND a form-urlencoded POST/PUT body —
both are percent-decoded and merged into the same table (query first,
body appended), up to `MAX_PARAMS = 32`. Names truncate at 31 chars,
values at 47. `body()` stays the raw untouched text, so a JSON body is
still readable.

Only `200`, `400`, `404` and `500` map to a real status line. **Any other
code silently goes out as `200 OK`** — don't rely on `401`, `403`, `409`
or `503` reaching the client.

`send`/`sendProgmem` take a null-terminated C string and truncate at the
first `0x00` byte — use `sendBinary` for anything that isn't text (images,
packed structs). It takes an explicit length, so embedded zero bytes are
safe. `filename`, if non-null, sets `Content-Disposition: attachment;
filename="..."` so a browser or `curl -O` saves the response under that
name instead of guessing one from the URL.

### `http::httpGet` / `http::httpPost`

```cpp
HttpResult httpGet (const char* url, int timeout_ms = 10000);
HttpResult httpPost(const char* url, const char* json, size_t json_len,
                    int timeout_ms = 10000);
```

`HttpResult{ bool success; int statusCode; char body[1024]; size_t bodyLen; }` —
`success == true` iff status is 2xx. `bool bodyContains(const char*)`
checks for a substring inside the (possibly truncated) body. HTTPS uses
the mbedTLS cert bundle on ESP32, libcurl on host.

### `ntp::ntp_*` (require ESP_PLATFORM)

| Function | Notes |
| -------- | ----- |
| `void ntp_init(const NtpConfig& = {})` | One-shot. Later calls are ignored until `ntp_stop()`. |
| `bool ensure_started(const NtpConfig& = {})` | STA-gated `ntp_init`. `false` when STA is down. Inline, safe to call on boot and on every reconnect. |
| `bool ensure_system_clock(const NtpConfig& = {})` | The full composition: `ensure_started` + `resync` + install `NtpTimeProvider` as the `ungula::core::time` provider + point emblogx timestamps at the same clock. Idempotent; the provider is installed once. |
| `void resync()` | Force an immediate re-poll. `ntp_init` is one-shot, so without this a mid-session reconnect waits up to `syncIntervalSec`. No-op before `ntp_init`. |
| `void ntp_stop()` | Stop SNTP. |
| `bool ntp_is_synced()` | True once the system clock passed 2020-01-01. |
| `time_t ntp_epoch()` | UTC epoch seconds, `0` until synced. |

`ensure_system_clock` lives in its own translation unit
(`ntp_system_clock.cpp`) so builds that exclude the `/ntp/` source group
never pull in the time-provider and logger wiring.

`NtpConfig{ const char* server = "pool.ntp.org"; const char* fallbackServer
= "time.google.com"; uint32_t syncIntervalSec = 3600; }` — no timezone
field; that lives in `ungula::core::time`.

### `ntp::NtpTimeProvider`

Implements `ungula::core::time::ITimeProvider`. Default constructor wires the real
backend; the second constructor takes `(NtpIsSyncedFn, NtpEpochFn,
LocalTickFn)` for host-test injection (any null pointer falls back to the
real backend). `setRefreshIntervalMs(int64_t)` (0 disables the cache),
`refreshIntervalMs()`. `nowMs()` returns full UTC epoch-ms; `isValid()`
returns `false` until the first NTP sync, which makes `ungula::core::time::now()`
fall back to local `millis()`.

---

## Lifecycle

- **WiFi**: pick exactly one of `ap_init` (AP+STA), `sta_init` + `sta_connect`
  (STA-only), or `espnow_init` (STA radio for ESP-NOW). All other
  `wifi::*` calls assume one of these has succeeded.
- **HttpServer**: `start` → `route(...)` × N → `enableWebSocket(path)` (optional)
  → `ready()` once. Routes registered after `ready()` are not picked up.
- **Pairing coordinator**: `loadPairedClients()` → `onClientPaired(...)` →
  `enablePairing()` (typically gated on a UI button) → drive `loop(nowMs)`
  every iteration → forward inbound packets to `handleReceived` first.
- **Pairing client**: `setScanChannels(...)` → `loadStoredPairing()` →
  if not stored, `startScanning()` → `loop(nowMs)` every iteration. The
  scan-channel pointer must outlive the client.
- **Connection manager**: build `ISessionProvider`, then
  `ConnectionManager::begin(nowMs)`, then `loop(nowMs)` plus
  `onMessageReceived` on every inbound coordinator message.
- **NTP**: WiFi STA must be connected before `ntp_init`. Easiest path is
  `ensure_system_clock()` on boot and again on every STA reconnect. Doing
  it by hand: `ntp_init` → construct `NtpTimeProvider` → 
  `ungula::core::time::setTimeProvider(&clock)` → `resync()` after each
  reconnect.
- **HTTP client**: stateless; safe to call from any task once WiFi has IP.

---

## Error handling

- `TransportError`: `Ok`, `SendFailed`, `NotInitialized`,
  `PeerNotFound`, `BufferFull`, `InvalidArgument`, `Timeout`. Functions
  return this synchronously; ACK failure for an attempted unicast also
  surfaces via `onSendComplete`. `EspNowTransport` only ever returns
  `Ok`, `SendFailed`, `NotInitialized` and `InvalidArgument` — do not
  branch on `PeerNotFound`, `BufferFull` or `Timeout`, nothing produces
  them. Sending to a MAC that was never added comes back as
  `SendFailed`.
- `HttpResult.success` is `false` on transport failure (`statusCode == 0`)
  and on any non-2xx response. Body is silently truncated to 1024 bytes.
- `IPreferences::getBytes` returning the wrong size or a CRC mismatch in
  `WifiConfigStore` falls back to `WifiConfig::createDefault()`.
- `ntp_epoch()` returns `0` until first sync. `NtpTimeProvider::isValid()`
  returns `false` until first sync, causing `ungula::core::time::now()` to fall
  back to `millis()`.
- Pairing client: when a `PAIRING_TIMEOUT_MS` elapses in `Responding`,
  `loop()` restarts the scan on its own — no application action needed
  and no error surfaces. There is no "pairing failed" callback: poll
  `getState()` / `isPaired()` if the UI must show progress.
  `PairingState::Failed`, `Broadcasting` and `Confirming` exist in the
  enum but are never entered — don't switch on them.
- `PairingClient::handleConfirm` and `EspNowSessionProvider::onReconnectAck`
  take the channel straight off the wire. An out-of-range channel is
  rejected by `setChannel`/`setPairedChannel` but the FSM still moves to
  "paired"/"reacquired", so a corrupt frame can leave the node believing
  it is connected on a channel it never switched to. The heartbeat
  timeout recovers it on the next cycle.

---

## Threading / timing / hardware notes

- Transport receive callbacks fire on the ESP-NOW WiFi task — keep them
  short. The buffer is only valid during the call; copy it if you need
  it later.
- **Nothing in this library is thread-safe.** There is not a single lock
  in `lib_net`. Every FSM (`PairingClient`, `PairingCoordinator`,
  `ConnectionManager`, `EspNowSessionProvider`) must be driven from one
  task only.
- That matters most for `handleReceived` / `onReconnectAck`. Calling them
  straight from the ESP-NOW receive callback puts them on the WiFi task
  while `loop()` mutates the same state on the application task, AND
  makes them write NVS (`storePairing`, `setPairedChannel`) from a
  context that must stay short. The safe pattern is to copy `(srcMac,
  data, len)` into a queue in the callback and drain it from the task
  that owns the FSMs.
- HTTP server handlers run on the httpd task (`CONFIG_HTTPD_STACK`,
  default 12288 bytes; raise via build flag if handlers build large
  responses on the stack).
- `wsBroadcast` / `wsPing` walk the client list unprotected while the
  httpd task can add or remove clients on connect/disconnect. Call them
  from one task and accept that a client dropping mid-broadcast can cost
  you a send. The server ignores inbound WebSocket frames by design — use
  REST POST endpoints for client→server commands.
- All `pairing` and `connection` `loop()` calls expect monotonic ms in
  `uint32_t`; pass `static_cast<uint32_t>(ungula::core::time::millis())`.
- `NtpTimeProvider` is not thread-safe either — `nowMs()` mutates the
  cache. Install at boot from one context; concurrent readers can race
  the re-anchor.
- `WifiConfigStore::save` calls into NVS — slow (tens of ms on first
  write). Don't call from ISRs or from a tight loop.
- AP+STA mode: connecting STA to a router forces the AP onto the router's
  channel. Document this for any ESP-NOW peers or pin the AP channel via
  `WifiApConfig.channel`.

---

## Internals not part of the public API

- `onDataRecvCb` / `onDataSentCb` in `esp32_esp_now_transport.cpp` — file-
  static C trampolines required by ESP-NOW. Not members, not reachable,
  never call manually. They dispatch to the single pair of callbacks
  registered through `onReceive` / `onSendComplete`, which are file-static
  too: a second `EspNowTransport` instance would overwrite the first's
  callbacks. Use one instance.
- `ungula/net/connection/reconnect_messages.h::RECONNECT_MAGIC`,
  `ungula/net/pairing/pairing_types.h::PAIRING_MAGIC` — wire constants.
  `EspNowSessionProvider` and the pairing classes own the magic. Do not
  emit raw `ReconnectProbe` / `PairingBeacon` from application code.
- `ungula/net/pairing/pairing_types.h::PREF_KEY_*` — internal NVS keys for the
  pairing namespace. Use `clearPairing()` / `unpairAll()` instead of
  poking them.
- `HttpRequest::impl_` / `params_` / `paramCount_` / `uri_` / `body_` and
  `HttpServer::impl_` / `routes_` / `routeCount_` / `notFoundHandler_` /
  `wsClientFds_` / `wsClientCount_` / `wsEnabled_` — public only because
  the platform implementation needs them. Never read or mutate from
  application code; use the accessors.
- `ungula/net/pairing/pairing_beacon.h` `PairingBeacon`, `PairingRequest`,
  `PairingConfirm` — wire structs. The `init()` / `isValid()` helpers
  exist for the pairing classes; application code only sees `MessageHeader`-
  framed traffic via `handleReceived` returning `true`.

---

## LLM usage rules

- Use only the symbols and include paths documented here. Don't infer
  extra public API from the source, and don't invent symbols — report the
  gap instead.
- Follow the use-case patterns above; keep the wiring and lifecycle order
  identical unless the task is explicitly about changing the API.
- Everything under `ungula/net/platform/` is internal.
- Pick exactly one WiFi initializer (`ap_init`, `sta_init`,
  `wifi_stack_up_sta` or `espnow_init`) at boot. Don't mix.
- Always call `HttpServer::ready()` after the last `route` or
  `enableWebSocket`. Routes added after `ready()` will not match.
- Forward every inbound transport message to
  `PairingCoordinator::handleReceived` (or the client equivalent) before
  application dispatch — they consume their own protocol packets.
- Pairing client: keep the scan-channel array alive for the life of the
  `PairingClient` (it's stored as a raw pointer, not copied).
- Prefer `httpGet` / `httpPost` for short responses. Anything over 1024
  bytes is silently truncated — don't use this client for large payloads.
- Use `NtpTimeProvider` + `ungula::core::time::setTimeProvider` rather than
  reading `ntp_epoch()` directly in application code; that way
  formatting, timezone, and sync-checks all flow through `ungula::core::time`.
- The `MessageHeader` helpers return pointers (`extractHeader`,
  `extractPayload`) and may return `nullptr`. Always null-check.
- Send unicast only after `addPeer` returns `Ok`. Skipping it gives
  `TransportError::SendFailed`, not `PeerNotFound`. `addPeer` return
  values are worth checking — the library's own call sites discard them.
- Validate the length before casting a received frame to any wire struct
  (`ReconnectAck`, `PairingBeacon`, …). Only `extractHeader` /
  `extractPayload` check it for you.
- Keep transport `onReceive` callbacks short — they run on the WiFi
  task. Copy the buffer if you need to defer work, and hand FSM work
  (`handleReceived`, `onReconnectAck`) to the application task.
- Don't include `<esp_now.h>` or `<esp_http_*.h>` directly from project
  code. The library wraps these.
