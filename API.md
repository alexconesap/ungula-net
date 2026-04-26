# UngulaNet (`lib_net`)

ESP32-only networking stack on top of ESP-IDF: WiFi (AP / STA / AP+STA),
ESP-NOW transport with multi-channel pairing, a transport-agnostic
connection manager, an HTTP+WebSocket server on a single port, an HTTP/S
client, and an SNTP-based time provider that plugs into UngulaCore's
`TimeControl`. No Arduino dependencies.

Depends on `UngulaCore` and `EmblogX`. Targets ESP32 only — host builds
stub the platform glue (HTTP client uses libcurl; WiFi/NTP/ESP-NOW APIs
are no-ops or compile-out).

Umbrella header: `<ungula_net.h>` — pulls in the most common
sub-headers (`comm/`, `wifi/`, `pairing/pairing_types.h`, `http/`,
`ntp/`). The `connection/` headers and the platform-specific
`pairing_coordinator.h` / `pairing_client.h` are not in the umbrella —
include them directly.

---

## Usage

### Use case: ESP-NOW only (no AP, no web server)

```cpp
#include <ungula_net.h>
#include <comm/esp_now_transport.h>

using namespace ungula;
using namespace ungula::comm;

EspNowTransport transport;

static void onMessage(const MacAddress& src, const uint8_t* data, uint16_t len) {
    const MessageHeader* hdr = extractHeader(data, len);
    if (hdr == nullptr) return;
    // hdr->messageType, extractPayload(data, len), payloadLength(len)
}

void setup() {
    wifi::espnow_init();          // WiFi STA, minimum required by ESP-NOW
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
    TimeControl::delayMs(1000);
}
```

When to use this: headless nodes that talk only to peers — no browser UI,
no internet.

### Use case: AP + REST + WebSocket portal

```cpp
#include <wifi/wifi_ap.h>
#include <http/http_server.h>

using ungula::http::HttpServer;
using ungula::http::HttpRequest;
using ungula::http::Method;

static HttpServer server;

static void handleStatus(HttpRequest& req) {
    req.sendJson(200, R"({"status":"ok"})");
}

void setup() {
    ungula::wifi::WifiApConfig cfg;
    cfg.ssid           = "MyDevice";
    cfg.password       = "secret123";
    cfg.channel        = ungula::wifi::WifiChannel::Ch6;
    cfg.maxConnections = 4;
    ungula::wifi::ap_init(cfg);

    server.start(80);
    server.route(Method::GET, "/api/status", handleStatus);
    server.enableWebSocket("/ws");
    server.ready();                       // call AFTER all routes
}

void loop() {
    static uint32_t last = 0;
    if (ungula::TimeControl::millis() - last >= 1000) {
        last = ungula::TimeControl::millis();
        const char* json = R"({"uptime":1})";
        server.wsBroadcast(json, std::strlen(json));
    }
}
```

When to use this: device with a captive web portal that pushes live
state to one or more browser tabs.

### Use case: HTTP / HTTPS client (push to cloud)

```cpp
#include <http/http_client.h>

namespace http = ungula::http;

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

### Use case: NTP-backed wall clock plugged into TimeControl

```cpp
#include <wifi/wifi_sta.h>
#include <ntp/ntp_client.h>
#include <ntp/ntp_time_provider.h>
#include <time/time_control.h>

void setup() {
    ungula::wifi::sta_init();
    ungula::wifi::WifiStaConfig sta;
    sta.ssid = "MyRouter"; sta.password = "...";
    ungula::wifi::sta_connect(sta);

    ungula::ntp::ntp_init();                          // SNTP up
    static ungula::ntp::NtpTimeProvider clock;        // program lifetime
    ungula::TimeControl::setTimeProvider(&clock);
    ungula::TimeControl::setTimezone(ungula::tz::Timezone::CET);
}

void log() {
    char ts[24];
    ungula::TimeControl::formatLocal(ts, sizeof(ts));   // "" until synced
}
```

When to use this: any time `TimeControl::now()` must return real
UTC-epoch-ms instead of monotonic-since-boot.

### Use case: Coordinator-side pairing (accept new clients)

```cpp
#include <comm/esp_now_transport.h>
#include <pairing/pairing_coordinator.h>
#include <preferences/core/esp32_preferences.h>

using namespace ungula;

static comm::EspNowTransport transport;
static Esp32Preferences       prefs;
static pairing::PairingCoordinator pair(transport, prefs, "pair_ns");

static void onRx(const comm::MacAddress& src, const uint8_t* data, uint16_t len) {
    if (pair.handleReceived(src, data, len)) return;
    // ... application messages
}

static void onPaired(const pairing::PairedClientEvent& ev) {
    // ev.mac, ev.deviceId
}

void setup() {
    wifi::espnow_init();
    transport.init();
    transport.setChannel(6);
    transport.onReceive(onRx);

    pair.loadPairedClients();
    pair.onClientPaired(&onPaired);
}

void onPairButton() { pair.enablePairing(); }

void loop() { pair.loop(TimeControl::millis()); }
```

### Use case: Client-side pairing (find a coordinator)

```cpp
#include <pairing/pairing_client.h>
#include <preferences/core/esp32_preferences.h>

using namespace ungula;

static comm::EspNowTransport transport;
static Esp32Preferences       prefs;
static pairing::PairingClient pair(transport, prefs, "pair_ns", /*deviceId=*/7);

static void onPaired(const comm::MacAddress& mac, uint8_t channel) {
    // pairing.getCoordinatorMac(), pairing.getPairedChannel()
}

void setup() {
    wifi::espnow_init();
    transport.init();

    static const uint8_t scanCh[] = {1, 6, 11};
    pair.setScanChannels(scanCh, 3);    // pointer must outlive pair
    pair.onPaired(&onPaired);

    auto stored = pair.loadStoredPairing();
    if (!stored.valid) pair.startScanning();
}

void loop() {
    pair.loop(TimeControl::millis());
}
```

### Use case: Connection lifecycle with reacquisition (ESP-NOW)

```cpp
#include <connection/connection_manager.h>
#include <connection/espnow_session_provider.h>

using namespace ungula;

static comm::EspNowTransport      transport;
static Esp32Preferences           prefs;
static pairing::PairingClient     pair(transport, prefs, "pair_ns", 7);

static void sendHeartbeat(const comm::MacAddress& coord, void* /*ctx*/) {
    // build + transport.send(coord, ...);
}

static EspNowSessionProvider session(transport, pair, &sendHeartbeat, nullptr);
static ConnectionConfig cfg;     // defaults are reasonable
static ConnectionManager conn(session, cfg);

void setup() {
    conn.begin(TimeControl::millis());
}

void onAnyMessageFromCoordinator() {
    conn.onMessageReceived(TimeControl::millis());
}

void loop() {
    conn.loop(TimeControl::millis());
}
```

When to use this: a node that must auto-recover after coordinator
reboots, channel changes, or transient ESP-NOW drops.

---

## Public types

| Type | Header | Purpose |
| ---- | ------ | ------- |
| `comm::ITransport` | `comm/i_transport.h` | Abstract transport |
| `comm::EspNowTransport` | `comm/esp_now_transport.h` | ESP-NOW backend |
| `comm::MacAddress` (POD) | `comm/transport_types.h` | 6-byte MAC value |
| `comm::TransportError` (enum) | `comm/transport_types.h` | OK/SEND_FAILED/… |
| `comm::MessageHeader` (packed, 8B) | `comm/message_header.h` | Wire header |
| `wifi::WifiChannel` (enum) | `wifi/wifi_channel.h` | Ch1..Ch13, ChAuto |
| `wifi::WifiApConfig` | `wifi/wifi_ap.h` | AP setup |
| `wifi::WifiStaConfig`, `WifiScanResult` | `wifi/wifi_sta.h` | STA + scanning |
| `wifi::WifiConfig`, `WifiConfigStore` | `wifi/wifi_config.h` | NVS-backed STA creds |
| `pairing::PairingCoordinator` | `pairing/pairing_coordinator.h` | Coordinator FSM |
| `pairing::PairingClient` | `pairing/pairing_client.h` | Client FSM |
| `pairing::PairedClientInfo`, `PairedClientEvent` | `pairing/pairing_coordinator.h` | Coordinator state |
| `pairing::StoredPairing` | `pairing/pairing_client.h` | Cached pairing |
| `pairing::PairingState` (enum) | `pairing/pairing_types.h` | Pairing FSM |
| `pairing::PairingBeacon/Request/Confirm` (packed, 8B) | `pairing/pairing_beacon.h` | Wire structs |
| `ConnectionManager` | `connection/connection_manager.h` | Connection FSM |
| `ConnectionConfig`, `ConnectionPolicy` | `connection/connection_config.h` | Tuning |
| `ConnMgrState` (enum) | `connection/connection_config.h` | FSM state |
| `ISessionProvider` | `connection/i_session_provider.h` | Transport adapter |
| `EspNowSessionProvider` | `connection/espnow_session_provider.h` | ESP-NOW adapter |
| `ReconnectProbe`, `ReconnectAck` (packed, 8B) | `connection/reconnect_messages.h` | Reacquisition wire |
| `http::HttpServer`, `HttpRequest` | `http/http_server.h` | HTTP+WS server |
| `http::Method`, `RouteHandler` | `http/http_server.h` | Routes |
| `http::HttpResult` | `http/http_client.h` | Client response |
| `ntp::NtpConfig` | `ntp/ntp_client.h` | NTP setup |
| `ntp::NtpTimeProvider` | `ntp/ntp_time_provider.h` | `ITimeProvider` adapter |

Constants worth knowing:

- `comm::TRANSPORT_MAX_PAYLOAD = 240` — single ESP-NOW message cap.
- `pairing::MAX_PAIRED_CLIENTS = 2` — coordinator slot count.
- `pairing::MAX_SCAN_CHANNELS = 13`, `BEACON_INTERVAL_MS = 100`,
  `CHANNEL_SCAN_DWELL_MS = 200`, `PAIRING_TIMEOUT_MS = 10000`.
- `http::MAX_ROUTES = 40`, `HttpServer::MAX_WS_CLIENTS = 4`,
  `HttpRequest::MAX_PARAMS = 10`, body buffer 384 bytes, body buffer in
  `HttpResult` 1024 bytes (truncated silently above this).
- `wifi::WIFI_MAX_SCAN_RESULTS = 16`,
  `WIFI_SSID_MAX_LEN = 33`, `WIFI_PASS_MAX_LEN = 65`.

---

## Public functions / methods

### `comm::ITransport`

| Member | Notes |
| ------ | ----- |
| `TransportError init()` | Bring up the radio. |
| `TransportError send(const MacAddress&, const uint8_t* data, uint16_t len)` | `len ≤ TRANSPORT_MAX_PAYLOAD`. Unicast requires `addPeer` first. Broadcast uses `MacAddress::broadcast()`. |
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

### `comm::MessageHeader` helpers (`comm/message_header.h`)

- `bool isValidHeader(const uint8_t* data, uint16_t len, uint8_t expectedVersion)`
- `const MessageHeader* extractHeader(const uint8_t* data, uint16_t len)` — returns `nullptr` on short buffer.
- `const uint8_t* extractPayload(const uint8_t* data, uint16_t len)` — returns `nullptr` if no payload.
- `uint16_t payloadLength(uint16_t totalLen)`

> Note: README snippets in earlier docs that called `extractHeader(data, len)` as a value-returning function or `isValidHeader` with two arguments are stale. The signatures above are authoritative.

### `wifi::ap_*` (require ESP_PLATFORM)

`bool ap_init(const WifiApConfig&)`, `const char* ap_get_ip()`,
`const char* ap_get_sta_ip()`, `bool ap_sta_connected()`,
`const char* ap_get_mac()`, `bool ap_is_active()`,
`WifiChannel ap_get_channel()`. `WifiApConfig{ssid, password, channel,
maxConnections}` defaults to `ChAuto` and 4 connections.

### `wifi::sta_*` (require ESP_PLATFORM)

`bool sta_init()`, `bool sta_connect(const WifiStaConfig&)` (blocking,
default 15 s timeout), `void sta_disconnect()`, `bool sta_is_connected()`,
`const char* sta_get_ip()`, `const char* sta_get_mac()`,
`WifiChannel sta_get_channel()`, `void sta_refresh_dns()`,
`uint32_t sta_get_cached_dns_main()`, `uint32_t sta_get_cached_dns_backup()`,
`uint8_t sta_scan(WifiScanResult*, uint8_t maxResults,
const char* const* prefixes = nullptr, uint8_t prefixCount = 0)`.

`bool wifi::espnow_init()` — STA-only minimum for ESP-NOW.

### `wifi::WifiConfigStore`

`WifiConfig load()` — returns defaults on missing/CRC-mismatch.
`void save(const WifiConfig&)`, `void clear()`. CRC32 over the blob in
NVS namespace given to the constructor.

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

### `pairing::PairingClient`

| Member | Notes |
| ------ | ----- |
| `PairingClient(comm::ITransport&, IPreferences&, const char* prefsNs, uint8_t deviceId)` |  |
| `StoredPairing loadStoredPairing()` |  |
| `void setScanChannels(const uint8_t* channels, uint8_t count)` | **Pointer-only** — array must outlive the client. |
| `void startScanning() / stopScanning()` |  |
| `bool isScanning() / isPaired()` |  |
| `PairingState getState()` |  |
| `void loop(uint32_t nowMs)` |  |
| `bool handleReceived(const comm::MacAddress& src, const uint8_t* data, uint16_t len, uint32_t nowMs)` | `true` ⇒ consumed. |
| `void onPaired(OnPairedCallback)` | `void(*)(const MacAddress&, uint8_t channel)`. |
| `const comm::MacAddress& getCoordinatorMac()`, `uint8_t getPairedChannel()`, `void setPairedChannel(uint8_t)` | Only valid when paired. |
| `uint8_t getDeviceId()`, `const uint8_t* getScanChannels()`, `uint8_t getScanChannelCount()`, `void clearPairing()` |  |

### `ConnectionManager` + `ISessionProvider`

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

`EspNowSessionProvider` constructor:
`(comm::ITransport&, pairing::PairingClient&, ProbeCallback, void* ctx)`
where `ProbeCallback = void(*)(const comm::MacAddress&, void*)`. Also
exposes `bool onReconnectAck(const ReconnectAck&)` to wire into the
receive path.

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
`void sendJson(int code, const char* json)`,
`bool hasParam(const char* name)`, `const char* param(const char* name)`
(empty string when missing), `const char* uri()`, `const char* body()`
(POST/PUT body, capped at 384 bytes).

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

`void ntp_init(const NtpConfig& = {})` (idempotent — safe to call again),
`void ntp_stop()`, `bool ntp_is_synced()`, `time_t ntp_epoch()` (0 until
sync).

`NtpConfig{ const char* server = "pool.ntp.org"; const char* fallbackServer
= "time.google.com"; uint32_t syncIntervalSec = 3600; }` — no timezone
field; that lives in `TimeControl`.

### `ntp::NtpTimeProvider`

Implements `ungula::ITimeProvider`. Default constructor wires the real
backend; the second constructor takes `(NtpIsSyncedFn, NtpEpochFn,
LocalTickFn)` for host-test injection (any null pointer falls back to the
real backend). `setRefreshIntervalMs(int64_t)` (0 disables the cache),
`refreshIntervalMs()`. `nowMs()` returns full UTC epoch-ms; `isValid()`
returns `false` until the first NTP sync, which makes `TimeControl::now()`
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
- **NTP**: WiFi STA must be connected before `ntp_init`. Construct
  `NtpTimeProvider` after `ntp_init` is called for the first time, then
  `TimeControl::setTimeProvider(&clock)`.
- **HTTP client**: stateless; safe to call from any task once WiFi has IP.

---

## Error handling

- `TransportError`: `OK`, `SEND_FAILED`, `NOT_INITIALIZED`,
  `PEER_NOT_FOUND`, `BUFFER_FULL`, `INVALID_ARGUMENT`, `TIMEOUT`. Functions
  return this synchronously; ACK failure for an attempted unicast also
  surfaces via `onSendComplete`.
- `HttpResult.success` is `false` on transport failure (`statusCode == 0`)
  and on any non-2xx response. Body is silently truncated to 1024 bytes.
- `IPreferences::getBytes` returning the wrong size or a CRC mismatch in
  `WifiConfigStore` falls back to `WifiConfig::createDefault()`.
- `ntp_epoch()` returns `0` until first sync. `NtpTimeProvider::isValid()`
  returns `false` until first sync, causing `TimeControl::now()` to fall
  back to `millis()`.
- Pairing FSM enters `PairingState::FAILED` on timeout
  (`PAIRING_TIMEOUT_MS`); the client must call `clearPairing()` and
  `startScanning()` to retry.

---

## Threading / timing / hardware notes

- Transport receive callbacks fire on the ESP-NOW WiFi task — keep them
  short. Copy the buffer if you need it past the callback.
- HTTP server handlers run on the httpd task (`CONFIG_HTTPD_STACK`,
  default 8192 bytes; raise via build flag if handlers build large
  responses on the stack).
- `wsBroadcast` walks the client list under an internal mutex; safe to
  call from any task. Ignores inbound WebSocket frames by design — use
  REST POST endpoints for client→server commands.
- All `pairing` and `connection` `loop()` calls expect monotonic ms in
  `uint32_t`; pass `(uint32_t)TimeControl::millis()`. Internally these
  FSMs do not protect themselves with locks — drive them from a single
  task.
- `NtpTimeProvider::nowMs()` is reentrant after first sync; the cache is
  protected by a single load/store sequence and is good enough for
  single-task readers. Install at boot from one context.
- `WifiConfigStore::save` calls into NVS — slow (tens of ms on first
  write). Don't call from ISRs or from a tight loop.
- AP+STA mode: connecting STA to a router forces the AP onto the router's
  channel. Document this for any ESP-NOW peers or pin the AP channel via
  `WifiApConfig.channel`.

---

## Internals not part of the public API

- `comm::EspNowTransport::onDataRecvCb` / `onDataSentCb` — static C
  trampolines required by ESP-NOW; never call manually.
- `connection/reconnect_messages.h::RECONNECT_MAGIC`,
  `pairing/pairing_types.h::PAIRING_MAGIC` — wire constants.
  `EspNowSessionProvider` and the pairing classes own the magic. Do not
  emit raw `ReconnectProbe` / `PairingBeacon` from application code.
- `pairing/pairing_types.h::PREF_KEY_*` — internal NVS keys for the
  pairing namespace. Use `clearPairing()` / `unpairAll()` instead of
  poking them.
- `HttpRequest::impl_` / `HttpServer::impl_` / `routes_` / `wsClientFds_` —
  exposed publicly for the platform implementation only. Never read or
  mutate from application code; treat the request object as the API.
- `pairing/pairing_beacon.h` `PairingBeacon`, `PairingRequest`,
  `PairingConfirm` — wire structs. The `init()` / `isValid()` helpers
  exist for the pairing classes; application code only sees `MessageHeader`-
  framed traffic via `handleReceived` returning `true`.

---

## LLM usage rules

- Pick exactly one WiFi initializer (`ap_init`, `sta_init`, or
  `espnow_init`) at boot. Don't mix.
- Always call `HttpServer::ready()` after the last `route` or
  `enableWebSocket`. Routes added after `ready()` will not match.
- Forward every inbound transport message to
  `PairingCoordinator::handleReceived` (or the client equivalent) before
  application dispatch — they consume their own protocol packets.
- Pairing client: keep the scan-channel array alive for the life of the
  `PairingClient` (it's stored as a raw pointer, not copied).
- Prefer `httpGet` / `httpPost` for short responses. Anything over 1024
  bytes is silently truncated — don't use this client for large payloads.
- Use `NtpTimeProvider` + `TimeControl::setTimeProvider` rather than
  reading `ntp_epoch()` directly in application code; that way
  formatting, timezone, and sync-checks all flow through `TimeControl`.
- The `MessageHeader` helpers return pointers (`extractHeader`,
  `extractPayload`) and may return `nullptr`. Always null-check.
- Send unicast only after `addPeer` succeeds; otherwise expect
  `TransportError::PEER_NOT_FOUND`.
- Keep transport `onReceive` callbacks short — they run on the WiFi
  task. Copy the buffer if you need to defer work.
- Don't include `<esp_now.h>` or `<esp_http_*.h>` directly from project
  code. The library wraps these.
