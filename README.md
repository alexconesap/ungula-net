# UngulaNet

> **High-performance embedded C++ libraries for ESP32, STM32 and other MCUs** — networking stack (WiFi AP/STA, ESP-NOW, HTTP server + WebSocket, HTTPS client, NTP). Currently supported targets: ESP32

> **LLM usage note:** if this library is consumed from a coding AI workflow, explicitly point the agent to `API.md` first. `API.md` is the LLM-facing contract (public API + examples + constraints) and avoids wasting time/tokens scanning source files and this human-oriented README.

> **Warning - Active Development:** This library is under active architecture work to support multiple projects in parallel. Its structure is not finalized yet and may change without notice while this work is in progress. Updates are currently frequent (often daily). Target for structural freeze and stable `v1.0.0`: **June 2026**.

Networking library for ESP32 projects. WiFi AP management, HTTP+WebSocket server, HTTP client, and NTP time synchronisation — all built on ESP-IDF, no Arduino networking dependencies.

The library compiles all components when `ESP_PLATFORM` is defined. The host project controls what it uses through its own `#include` directives and project-level guards — the library does not impose feature flags.

## Table of Contents

- [C++ Compatibility](#c-compatibility)
- [Compile flags](#compile-flags)
- [WiFi AP](#wifi-ap)
  - [WiFi STA](#wifi-sta)
  - [Storing STA credentials](#storing-sta-credentials)
- [ESP-NOW Initialization](#esp-now-initialization)
- [HTTP + WebSocket Server](#http--websocket-server)
  - [Starting the server](#starting-the-server)
  - [Registering routes](#registering-routes)
  - [Serving static content from PROGMEM](#serving-static-content-from-progmem)
  - [Sending binary data](#sending-binary-data)
  - [WebSocket broadcast](#websocket-broadcast)
  - [HttpRequest API](#httprequest-api)
  - [Server configuration](#server-configuration)
- [HTTP Client](#http-client)
  - [GET request](#get-request)
  - [POST request (JSON)](#post-request-json)
  - [Timeout control](#timeout-control)
  - [HttpResult](#httpresult)
- [Pairing (`ungula/net/pairing/`)](#pairing-ungulanetpairing)
  - [Multi-Channel Pairing for ESP-NOW Networks](#multi-channel-pairing-for-esp-now-networks)
  - [Connection lifecycle](#connection-lifecycle)
- [Communication (`ungula/net/comm/`)](#communication-ungulanetcomm)
  - [Sending and Receiving Messages](#sending-and-receiving-messages)
  - [Writing Your Own Transport](#writing-your-own-transport)
  - [MessageHeader](#messageheader)
- [NTP Time Synchronisation](#ntp-time-synchronisation)
  - [API](#api)
  - [Plug NTP into the time API (`ungula/net/ntp/ntp_time_provider.h`)](#plug-ntp-into-the-time-api-ungulanetntpntp_time_providerh)
- [Testing](#testing)
  - [Prerequisites](#prerequisites)
  - [Run the tests](#run-the-tests)
  - [What's tested](#whats-tested)
- [Dependencies](#dependencies)
- [Acknowledgements](#acknowledgements)
- [License](#license)
- [Arduino CLI symlink note (rarely relevant)](#arduino-cli-symlink-note-rarely-relevant)

## C++ Compatibility

- **Own source minimum**: `C++20`.
- **Effective minimum for consumers**: `C++20`.
- **Dependency impact**: Own source uses designated initializers in `lib_net/src`, which requires `C++20`.

## Compile flags

Every platform-specific `.cpp` is wrapped in a build guard, so exactly one
implementation of each seam links. With no flag set the seams have no
definition and the link fails — deliberately, so nothing falls back to a
stub by accident.

| Flag | What it enables | Who needs it |
| --- | --- | --- |
| `ESP_PLATFORM` | ESP-IDF implementations (ESP-NOW, WiFi, httpd, esp_http_client, SNTP) | All ESP32 nodes |
| `UNGULA_NET_MOCK` | Host stubs — ESP-NOW, WiFi, HTTP server and NTP become no-ops reporting "down" | Desktop / gtest builds |
| `UNGULA_NET_CURL` | Real libcurl HTTP client | Desktop tests hitting real endpoints |
| `CONFIG_HTTPD_STACK` | httpd task stack size in bytes (default 12288) | Override if handlers need more stack |
| `WIFI_NVS_NAMESPACE_VALUE` | Default NVS namespace for `WifiConfigStore` (default `"main_wifi"`) | Projects sharing a device |

Example build flags:

```text
-DESP_PLATFORM
```

The host stub for STA is partial: `sta_scan()`, `sta_get_cached_dns_main()`
and `sta_get_cached_dns_backup()` have no mock implementation, so host code
calling them will not link.

## WiFi AP

Sets up the ESP32 in AP+STA mode so you can host a local network and still use ESP-NOW at the same time.

```cpp
#include <ungula/net/wifi/wifi_ap.h>
#include <emblogx/logger.h>

using namespace ungula::net::wifi;

WifiApConfig config;
config.ssid           = "MyDevice";
config.password       = "secret123";
config.channel        = WifiChannel::Ch6;
config.maxConnections = 4;

if (ap_init(config)) {
    log_info("AP ready at %s", ap_get_ip());  // "192.168.4.1"
}
```

`config.ssid` has no default — set it. `ap_init()` does not check for
`nullptr` and will crash on the default-constructed config. An empty
password gives an open AP, anything else gives WPA2-PSK.

| Function | Returns | Description |
| --- | --- | --- |
| `ap_init(config)` | `bool` | Initialize WiFi AP+STA mode |
| `wifi_stack_up_sta()` | `bool` | STA mode started but not connected — the minimum PHY for ESP-NOW without a SoftAP |
| `ap_get_ip()` | `const char*` | AP IP address |
| `ap_get_sta_ip()` | `const char*` | STA IP address, `"0.0.0.0"` when not connected |
| `ap_sta_connected()` | `bool` | Whether the STA side has an IP |
| `ap_get_mac()` | `const char*` | AP interface MAC as `"AA:BB:CC:DD:EE:FF"` |
| `ap_is_active()` | `bool` | Whether AP is running |
| `ap_get_channel()` | `WifiChannel` | Effective channel in use |

### WiFi STA

`<ungula/net/wifi/wifi_sta.h>` connects the station interface to an
external router, with optional prefix-filtered scanning.

```cpp
#include <ungula/net/wifi/wifi_sta.h>

using namespace ungula::net::wifi;

WifiStaConfig sta;
sta.ssid             = "MyRouter";
sta.password         = "hunter2";
sta.connectTimeoutMs = 15000;

sta_init();
if (sta_connect(sta)) {          // blocks until connected or timeout
    log_info("STA up at %s", sta_get_ip());
}
```

| Function | Returns | Description |
| --- | --- | --- |
| `sta_init()` | `bool` | STA-only mode, no AP |
| `sta_connect(config)` | `bool` | Blocking connect, auto-retries on transient failures |
| `sta_disconnect()` | `void` | Voluntary disconnect (suppresses auto-reconnect) |
| `sta_is_connected()` | `bool` | Has a valid IP |
| `sta_get_ip()` / `sta_get_mac()` | `const char*` | Current IP / MAC string |
| `sta_get_channel()` | `WifiChannel` | Channel the STA settled on |
| `sta_refresh_dns()` | `void` | Re-assert DNS + default route after a DHCP renewal |
| `sta_get_cached_dns_main()` / `_backup()` | `uint32_t` | DNS cached at first connect, IPv4 network byte order |
| `sta_scan(results, maxResults, prefixes, prefixCount)` | `uint8_t` | Networks found, capped at `WIFI_MAX_SCAN_RESULTS` (16) |

Pick exactly one radio bring-up per firmware: `ap_init()` (AP+STA),
`sta_init()` (STA only), `wifi_stack_up_sta()`, or `espnow_init()`.

### Storing STA credentials

`WifiConfigStore` (`<ungula/net/wifi/wifi_config.h>`) keeps SSID, password
and an enable flag in one CRC32-protected NVS blob. A corrupted blob falls
back to defaults instead of returning garbage.

```cpp
#include <ungula/net/wifi/wifi_config.h>

ungula::net::wifi::WifiConfigStore store(prefs);      // or (prefs, "icb_wifi")

auto cfg = store.load();                              // defaults if absent/corrupt
if (cfg.hasCredentials()) { /* sta_connect(...) */ }

cfg.enabled = true;
std::strncpy(cfg.ssid, "MyRouter", sizeof(cfg.ssid) - 1);
store.save(cfg);
store.clear();                                        // back to defaults
```

The namespace defaults to the `WIFI_NVS_NAMESPACE_VALUE` build define
(`"main_wifi"`). Override it per project rather than per call site.

## ESP-NOW Initialization

For nodes that only need ESP-NOW (no web server, no AP), use `espnow_init()` to bring up the WiFi radio in STA mode -- the minimum required for ESP-NOW to work.

> **MANDATORY on ESP-IDF**: NVS must be initialised **before** `espnow_init()`.
> The WiFi driver persists calibration data through NVS and fails with
> `ESP_ERR_NVS_NOT_INITIALIZED` (reboot loop) otherwise. Call
> `ungula::core::preferences::initStorage()` (from `lib`) first — it handles
> the erase-and-retry path for a fresh / version-mismatched partition.
> Arduino-ESP32 did this implicitly at startup, so Arduino sketches never
> saw the requirement; pure ESP-IDF projects do.

```cpp
#include <ungula/core/preferences/preferences.h>  // initStorage()
#include <ungula/net/wifi/wifi_espnow.h>

using namespace ungula::net::wifi;

void setup() {
    // REQUIRED on ESP-IDF — must come first.
    if (!ungula::core::preferences::initStorage()) {
        // handle error
    }
    if (!espnow_init()) {
        // handle error
    }
    // ESP-NOW transport is now ready to use
}
```

| Function | Returns | Description |
| --- | --- | --- |
| `espnow_init()` | `bool` | Initialize WiFi in STA mode for ESP-NOW only |

No AP is started, no HTTP server, no web UI. This is the right choice for headless nodes that communicate exclusively via ESP-NOW.

## HTTP + WebSocket Server

*Requires `-DESP_PLATFORM`*

A unified HTTP and WebSocket server built on ESP-IDF `httpd`. One server, one port, both REST routes and WebSocket on the same instance. No Arduino WebServer dependency.

### Starting the server

```cpp
#include <ungula/net/http/http_server.h>
#include <emblogx/logger.h>

ungula::net::http::HttpServer server;

void setup() {
    ap_init(apConfig);

    server.start(80);
    registerRoutes(server);        // all route() calls
    server.enableWebSocket("/ws");
    server.ready();                // MUST come last
}
```

`ready()` registers the wildcard dispatchers that route requests to your
handlers. Call it once, after the last `route()` and after
`enableWebSocket()`. Without it nothing matches; routes added after it are
never seen.

### Registering routes

Routes are plain function pointers. The server dispatches incoming requests to the matching handler based on method + path.

```cpp
#include <ungula/net.h>
#include <emblogx/logger.h>

using Req = ungula::net::http::HttpRequest;
using Method = ungula::net::http::Method;

static void handleStatus(Req& req) {
    req.sendJson(200, R"({"status":"ok","uptime":12345})");
}

static void handleReboot(Req& req) {
    req.sendJson(200, R"({"status":"rebooting"})");
    requestReboot();
}

static void handleUpdateSetting(Req& req) {
    if (req.hasParam("temp")) {
        int temp = atoi(req.param("temp"));
        setTemperature(temp);
    }
    req.sendJson(200, R"({"status":"ok"})");
}

static void handlePostCommand(Req& req) {
    // POST body is available via req.body()
    const char* json_body = req.body();
    processCommand(json_body);
    req.sendJson(200, R"({"status":"ok"})");
}

void registerRoutes(ungula::net::http::HttpServer& server) {
    server.route(Method::GET,  "/api/status",   handleStatus);
    server.route(Method::POST, "/api/reboot",   handleReboot);
    server.route(Method::PUT,  "/api/settings", handleUpdateSetting);
    server.route(Method::POST, "/api/command",  handlePostCommand);
    server.setNotFoundHandler([](Req& req) {
        req.send(404, "text/plain", "Not found");
    });
}
```

The enum is `Method::GET`, `POST`, `PUT`, `DELETE_` (trailing underscore —
`DELETE` is a macro on some SDKs). Paths are stored by pointer, so pass
string literals or something else that outlives the server.

Only status codes 200, 400, 404 and 500 map to a real status line;
anything else goes out as `200 OK`.

### Serving static content from PROGMEM

Web portal HTML, CSS, and JS can be stored in flash and served directly:

```cpp
#include <pgmspace.h>

static const char MY_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html><body><h1>Hello</h1></body></html>
)rawhtml";

static void handlePortal(Req& req) {
    req.sendProgmem(200, "text/html", MY_HTML);
}

server.route(Method::GET, "/", handlePortal);
```

### Sending binary data

`send()` and `sendProgmem()` take a null-terminated C string and truncate
at the first `0x00` byte — fine for HTML/JSON/text, wrong for anything
that isn't text. Use `sendBinary()` instead, which takes an explicit
length. `lib_display`'s screen-capture feature uses it to serve the live
framebuffer as a downloadable `.bmp`:

```cpp
#include <ungula/net.h>
#include <ungula/display/gfx_core.h>

static void handleScreenshot(Req& req) {
    uint8_t* data = nullptr;
    size_t len = 0;
    if (ungula::display::gfx_capture_screen_bmp(&data, &len) != ungula::display::GfxCaptureStatus::Ok) {
        req.send(500, "text/plain", "capture failed");
        return;
    }
    // filename sets Content-Disposition, so `curl -O` saves it as screen.bmp
    req.sendBinary(200, "image/bmp", data, len, "screen.bmp");
    ungula::display::gfx_free_capture(data);
}

server.route(Method::GET, "/api/screenshot", handleScreenshot);
```

Response size is not a problem here — `httpd_resp_send()` takes an explicit
length and loops over partial socket writes internally, so a few hundred KB
goes out in one call with no chunking.

### WebSocket broadcast

Push real-time updates to all connected browser clients:

```cpp
#include <ungula/net.h>

server.enableWebSocket("/ws");

// Later, when something changes:
const char* json = R"({"temp":350,"mode":"ready"})";
int clients_sent = server.wsBroadcast(json, strlen(json));
```

The WebSocket is broadcast-only — the server ignores incoming messages from clients. This is by design: commands go through REST POST endpoints, status updates push through WebSocket.

### HttpRequest API

| Method | Description |
| --- | --- |
| `req.send(code, contentType, body)` | Send a response |
| `req.sendProgmem(code, contentType, data)` | Send from flash (PROGMEM) |
| `req.sendBinary(code, contentType, data, len, filename = nullptr)` | Send `len` bytes as-is — safe for embedded `0x00` (images, packed structs). `filename` sets `Content-Disposition: attachment`. |
| `req.sendJson(code, json)` | Convenience: send JSON response |
| `req.hasParam("name")` | Check if a parameter exists |
| `req.param("name")` | Get a parameter value, `""` when missing |
| `req.body()` | Get POST/PUT request body (raw, up to 768 bytes) |
| `req.uri()` | Get the request path, query string stripped |

`param()` covers both the query string and a form-urlencoded POST/PUT
body — both are percent-decoded and merged into one table (query first,
body appended). A JSON body simply adds no parameters, and `body()`
returns it untouched either way.

### Server configuration

The httpd task stack defaults to 12288 bytes (`CONFIG_HTTPD_STACK`). Each
request puts an `HttpRequest` (~3.4 KB of parameter and body buffers) on
that stack, so if your handlers also build large JSON there, raise it:

```text
-DCONFIG_HTTPD_STACK=16384
```

Limits: 40 routes, 4 WebSocket clients, 32 parameters per request
(names 31 chars, values 47), 768-byte request body, 96-byte URI.

Nothing in the server is locked. Handlers run on the httpd task while
`wsBroadcast()` typically runs on your application task — keep shared
state out of it or protect it yourself.

## HTTP Client

*Requires `-DESP_PLATFORM`*

Simple GET and POST requests for pushing data to cloud APIs or fetching remote resources.

On ESP32, uses ESP-IDF `esp_http_client`. On desktop (for testing), uses libcurl.

### GET request

```cpp
#include <ungula/net/http/http_client.h>
#include <emblogx/logger.h>

auto result = ungula::net::http::httpGet("https://api.example.com/health");
if (result.success) {
    log_info("Server responded %d: %s", result.statusCode, result.body);
}
```

### POST request (JSON)

```cpp
#include <ungula/net.h>
#include <emblogx/logger.h>

const char* json = R"({"device":"node-1","temp":350,"status":"ready"})";
auto result = ungula::net::http::httpPost(
    "https://api.example.com/status",
    json, strlen(json)
);

if (result.success) {
    log_info("Status pushed OK (%d)", result.statusCode);
} else {
    log_warn("Push failed: status=%d", result.statusCode);
}
```

### Timeout control

Both `httpGet` and `httpPost` accept an optional timeout in milliseconds (default 10 seconds):

```cpp
#include <ungula/net.h>
#include <emblogx/logger.h>

// 3-second timeout for a health check
auto result = ungula::net::http::httpGet("https://api.example.com/ping", 3000);
```

### HttpResult

| Field | Type | Description |
| --- | --- | --- |
| `success` | `bool` | True if HTTP status 2xx |
| `statusCode` | `int` | HTTP response code (200, 404, 500, etc.) |
| `body` | `char[1024]` | Response body (truncated if larger) |
| `bodyLen` | `size_t` | Actual bytes received (up to buffer size) |
| `bodyContains(str)` | `bool` | Check if body contains a substring |

The body buffer is 1024 bytes. Responses larger than that are silently truncated — no crash, no allocation. This is intentional for embedded use where you typically only need a short JSON response or a status check.

## Pairing (`ungula/net/pairing/`)

### Multi-Channel Pairing for ESP-NOW Networks

The pairing system lets a coordinator (e.g. a central controller) discover and pair with client nodes across multiple WiFi channels. Once paired, the MAC and channel are stored in NVS so they survive reboots.

**Coordinator side** (the device that accepts connections):

```cpp
#include <ungula/net/pairing/pairing_coordinator.h>
#include <ungula/core/time/time.h>
#include <emblogx/logger.h>

using namespace ungula::net;

pairing::PairingCoordinator pairing(transport, prefs, "pair_ns");

void setup() {
    pairing.loadPairedClients();

    // When a new node pairs with us
    pairing.onClientPaired([](const pairing::PairedClientEvent& ev) {
        log_info("Node %d paired", static_cast<int>(ev.deviceId));
    });
}

void onUserPressedPairButton() {
    pairing.enablePairing();  // Starts broadcasting beacons
}

void loop() {
    pairing.loop(static_cast<uint32_t>(ungula::core::time::millis()));
}

// In your application task, NOT in the ESP-NOW receive callback:
void onMessage(const comm::MacAddress& src, const uint8_t* data, uint16_t len) {
    if (pairing.handleReceived(src, data, len)) return;  // consumed by pairing
    // ... handle application messages
}
```

`handleReceived` also answers reconnect probes from already-paired
clients, whether or not pairing mode is on — that is how a node that lost
the channel finds the coordinator again. Probes from unknown MACs are
dropped.

Two slots (`MAX_PAIRED_CLIENTS = 2`). A request from a device that already
holds a slot updates it; otherwise the first free slot is used, and
failing that a slot with the same device ID is recycled.
`unpairClient(deviceId)` frees one slot, `unpairAll()` frees both.

**Client side** (the device that joins):

```cpp
#include <ungula/net/pairing/pairing_client.h>
#include <ungula/core/time/time.h>
#include <emblogx/logger.h>

using namespace ungula::net;

pairing::PairingClient pairing(transport, prefs, "pair_ns", MY_DEVICE_ID);

void setup() {
    // setScanChannels stores the POINTER — the array must outlive the
    // client. A local array here is a dangling pointer. Either use a
    // static one or the built-in default set from <wifi/scan_channels.h>.
    pairing.setScanChannels(wifi::DEFAULT_SCAN_CHANNELS,
                            wifi::DEFAULT_SCAN_CHANNEL_COUNT);  // {1, 6, 11}

    pairing.onPaired([](const comm::MacAddress& mac, uint8_t ch) {
        log_info("Paired with coordinator %s on channel %d", mac.c_str(), static_cast<int>(ch));
    });

    // After setScanChannels: loadStoredPairing validates the stored
    // channel against the scan list and falls back to its first entry.
    auto stored = pairing.loadStoredPairing();
    if (!stored.valid) {
        pairing.startScanning();  // No stored pairing, start looking
    }
}

void loop() {
    pairing.loop(static_cast<uint32_t>(ungula::core::time::millis()));
}
```

If the coordinator does not confirm within `PAIRING_TIMEOUT_MS` (10 s),
`loop()` restarts the scan by itself — there is no failure callback and
the state never becomes `Failed`. Poll `getState()` / `isPaired()` if the
UI needs to show progress.

### Connection lifecycle

Once paired, `ConnectionManager`
(`<ungula/net/connection/connection_manager.h>`) watches the heartbeat and
recovers when the coordinator disappears: healthy → degraded (grace
period) → probing the last known channel → scanning all pairing channels.
It is transport-agnostic; `EspNowSessionProvider` supplies the ESP-NOW
half (channel hopping, reconnect probes). Timings live in
`ConnectionConfig` — defaults are 2 s heartbeat timeout, 500 ms degraded
grace, 5 probes on the known channel, 500 ms between broad probes, 3 s
boot grace. See `API.md` for the full wiring.

## Communication (`ungula/net/comm/`)

### Sending and Receiving Messages

`ITransport` is the transport interface. You code against it, and the actual implementation (ESP-NOW, or anything else) is injected at setup time. This means your application logic never depends on a specific radio or protocol.

The ESP-NOW implementation is `EspNowTransport`. Here is a complete example — a coordinator that broadcasts a heartbeat every second and prints anything it receives:

```cpp
#include <ungula/net/comm/esp_now_transport.h>
#include <ungula/net/comm/message_header.h>
#include <emblogx/logger.h>
#include <emblogx/sinks/console_sink.h>

using namespace ungula::net::comm;

EspNowTransport transport;
static emblogx::ConsoleSink g_console;

// This runs on the WiFi task every time a message arrives — keep it short.
void onMessage(const MacAddress& src, const uint8_t* data, uint16_t len) {
    const MessageHeader* header = extractHeader(data, len);
    if (header == nullptr) return;   // frame shorter than the header
    log_info("Got message type %d from peer", static_cast<int>(header->messageType));
}

void setup() {
    emblogx::register_sink(&g_console);
    emblogx::init();

    transport.init();
    transport.setChannel(6);
    transport.onReceive(onMessage);
}

void loop() {
    // Build a heartbeat message
    uint8_t buf[sizeof(MessageHeader)];
    MessageHeader hdr = {};
    hdr.protocolVersion = 1;
    hdr.messageType = 0x01;  // your app-defined type
    memcpy(buf, &hdr, sizeof(hdr));

    // Broadcast to all peers on the channel
    transport.send(MacAddress::broadcast(), buf, sizeof(buf));
    ungula::core::time::delayMs(1000);
}
```

For unicast (sending to a specific device), you need to register the peer first:

```cpp
#include <ungula/net.h>
#include <emblogx/logger.h>

// Requires emblogx logger initialized in setup()

MacAddress peer = MacAddress::fromBytes(peerMacBytes);
transport.addPeer(peer, 6);  // channel 6

auto err = transport.send(peer, buf, len);
if (err != TransportError::Ok) {
    log_error("Send failed");
}
```

Broadcasts need the broadcast address added as a peer too — ESP-NOW will
not send to a MAC it does not know.

`TransportError` declares seven values but `EspNowTransport` only ever
returns `Ok`, `SendFailed`, `NotInitialized` and `InvalidArgument`. Don't
branch on `PeerNotFound`, `BufferFull` or `Timeout`; sending to an
unregistered peer comes back as `SendFailed`.

`shutdown()` releases the radio (calls `esp_now_deinit()`) so another
subsystem — an OTA download over STA, for instance — can take it. The
base-class default is a no-op, so transports needing no teardown just
inherit it. Call `init()` again to come back.

### Writing Your Own Transport

If you need something other than ESP-NOW (BLE, LoRa, serial, a mock for testing), implement `ITransport`:

```cpp
#include <ungula/net.h>

class MyLoRaTransport : public ungula::net::comm::ITransport {
public:
    TransportError init() override { /* ... */ }
    TransportError send(const MacAddress& dst, const uint8_t* data, uint16_t len) override { /* ... */ }
    void onReceive(TransportReceiveCallback cb) override { receiveCb_ = cb; }
    // ... rest of the interface
};
```

Then pass it to your application code the same way. Nothing changes downstream.

### MessageHeader

Every message starts with an 8-byte header. Utility functions let you pull
it apart. They are the only length-checked helpers in the library — the
extractors return `nullptr` on a short buffer, so always check.

```cpp
#include <ungula/net.h>

constexpr uint8_t PROTOCOL_VERSION = 1;

if (!isValidHeader(data, len, PROTOCOL_VERSION)) {
    return;  // too short, or a version we don't speak
}

const MessageHeader* hdr = extractHeader(data, len);      // nullptr if len < 8
const uint8_t* payload   = extractPayload(data, len);     // nullptr if no payload
uint16_t payloadLen      = payloadLength(len);            // 0 if no payload
```

| Function | Returns |
| --- | --- |
| `isValidHeader(data, len, expectedVersion)` | `bool` — length OK and version matches |
| `extractHeader(data, len)` | `const MessageHeader*`, `nullptr` if `len < 8` |
| `extractPayload(data, len)` | `const uint8_t*`, `nullptr` if `len <= 8` |
| `payloadLength(totalLen)` | `uint16_t`, `0` if `totalLen <= 8` |

| Field | Type | Description |
| --- | --- | --- |
| `protocolVersion` | `uint8_t` | Protocol version |
| `messageType` | `uint8_t` | Application-defined type |
| `sourceDeviceId` | `uint8_t` | Sender device ID |
| `sequenceNumber` | `uint8_t` | Rolling sequence (0-255) |
| `flags` | `uint8_t` | Bit 0: requiresAck, Bit 1: isAck |
| `reserved[3]` | `uint8_t[]` | Must be zero |

## NTP Time Synchronisation

*Requires `-DESP_PLATFORM`*

NTP is a **time source** — nothing else. Its sole job is to bring up the SNTP service and hand back the current UTC epoch in seconds. Timezone offsetting and string formatting are not its concern; those live in `ungula::core::time` and `time_format` respectively (`UngulaCore`), which apply equally whether the source is NTP, an RTC chip, a manual `setTime()`, or a fake. Same architectural rule applies to any future RTC backend: it produces an epoch, full stop.

### API

```cpp
#include <ungula/net/ntp/ntp_client.h>

namespace ntp = ungula::net::ntp;

ntp::NtpConfig cfg;          // pool.ntp.org by default, 1 h re-sync
ntp::ntp_init(cfg);

if (ntp::ntp_is_synced()) {
    time_t utcEpoch = ntp::ntp_epoch();   // raw seconds since 1970-01-01 UTC
}
```

| Function | Returns | Description |
| --- | --- | --- |
| `ntp_init(config)` | `void` | Start SNTP service. One-shot — later calls do nothing until `ntp_stop()`. |
| `ensure_started(config)` | `bool` | Start SNTP only when STA is connected. Returns `false` if STA is down, `true` once ntp_init() succeeds. Safe to call freely on boot or reconnect paths. |
| `ensure_system_clock(config)` | `bool` | The whole composition: `ensure_started` + `resync` + install `NtpTimeProvider` as the system clock + point emblogx timestamps at it. Idempotent. |
| `resync()` | `void` | Force an immediate re-poll. Needed on reconnect, since `ntp_init` is one-shot and the next scheduled sync can be an hour out. |
| `ntp_stop()` | `void` | Stop the SNTP service. |
| `ntp_is_synced()` | `bool` | True once the clock has been set by NTP. |
| `ntp_epoch()` | `time_t` | Current UTC epoch in seconds (0 if not synced). |

For the common case — "NTP is the wall clock" — call
`ensure_system_clock()` on boot and again on every STA reconnect and skip
the manual wiring below. It lives in its own translation unit so builds
that exclude the `ntp/` sources never pull in the logger glue.

`NtpConfig` has three fields: `server`, `fallbackServer`, `syncIntervalSec`. There is no `utcOffsetSeconds` here — TZ is owned by `ungula::core::time::setTimezone()`.

WiFi STA must be connected before calling `ntp_init()` so the DNS resolver can reach the NTP server. `ensure_started()` handles this automatically — it returns `false` when STA is down and calls `ntp_init()` only once the link is live. Prefer `ensure_started()` on reconnect paths where the STA may not be up yet. On desktop hosts all functions are stubbed (always return "not synced").

### Plug NTP into the time API (`ungula/net/ntp/ntp_time_provider.h`)

`NtpTimeProvider` is the `ITimeProvider` adapter that wires the NTP source into the system clock. Two lines:

```cpp
#include <ungula/net/ntp/ntp_client.h>
#include <ungula/net/ntp/ntp_time_provider.h>
#include <ungula/core/time/time.h>

ungula::net::ntp::ensure_started();                    // start SNTP once STA is up
static ungula::net::ntp::NtpTimeProvider ntpClock;     // lives for program lifetime
ungula::core::time::setTimeProvider(&ntpClock);  // ungula::core::time::now() routes through NTP
```

After this:

- `ungula::core::time::now()` / `nowUtc()` returns the NTP-aligned UTC timestamp as 64-bit epoch-ms.
- `ungula::core::time::nowLocal()` / `nowInTz(offset)` apply the configured timezone shift.
- `ungula::core::time::formatUtc()` / `formatLocal()` print the wall-clock string. **All formatting goes through `ungula::core::time`, not through the NTP client.**
- Until NTP syncs, the provider reports `isValid() == false` and `ungula::core::time::now()` falls back to local `millis()` automatically.

```cpp
#include <ungula/net.h>

ungula::core::time::setTimezone(ungula::core::time::tz::Timezone::CET);  // device in Barcelona

char ts[24];
ungula::core::time::formatLocal(ts, sizeof(ts));   // "2026-04-23 15:32:11"
ungula::core::time::formatUtc(ts, sizeof(ts));     // "2026-04-23 14:32:11"
```

#### Caching

The provider anchors on one `ntp_epoch()` read, then for the next `refreshIntervalMs` ms replies via pure arithmetic (`anchor_epoch_ms + (millis() - anchor_tick)`). After the TTL expires it re-anchors on the next call, absorbing whatever drift SNTP has corrected in the background. Default TTL is 60 s.

```cpp
#include <ungula/net.h>

ntpClock.setRefreshIntervalMs(10000);   // re-anchor every 10 s
ntpClock.setRefreshIntervalMs(0);        // disable cache — every call refetches
```

This makes the `now()` hot path cheap to call hundreds of times per second (e.g., from the logger) without worrying about the backend. `ntp_epoch()` itself is already a cheap `time()` syscall — the cache is defensive insurance, not a fix for an existing hotspot.

The cache is not locked. `nowMs()` writes to it, so install the provider at boot from one context and don't expect concurrent readers to be safe.

#### Testing hook

`NtpTimeProvider` has a second constructor that takes function-pointer seams for `ntp_is_synced`, `ntp_epoch`, and the local monotonic tick. Tests inject fakes; production code uses the default constructor and the real backend.

```cpp
#include <ungula/net.h>

ungula::net::ntp::NtpTimeProvider fake(&myIsSynced, &myEpoch, &myLocalTick);
```

## Testing

Two host suites run on desktop (macOS/Linux): the HTTP client against real
endpoints via libcurl, and `NtpTimeProvider` with injected fake clocks.

### Prerequisites

- CMake 3.16+
- A C++17 compiler is enough for the host test targets (they compile only
  the curl client and the NTP provider). The library itself needs C++20.
- libcurl development headers (`brew install curl` on macOS)
- Internet access — the HTTP suite hits external APIs and fails without it
- `UngulaCore` as a sibling `lib/` directory, or `-DUSE_LOCAL_DEPS=OFF` to
  fetch it from GitHub

### Run the tests

```shell
cd tests
./1_build.sh     # configure cmake (only needed once)
./2_run.sh       # build and run all tests
```

### What's tested

`test_http_client` — 16 integration tests against Postman Echo and
httpbin.org:

| Category | Tests |
| --- | --- |
| GET requests | 200 response, query params echoed, headers endpoint |
| POST requests | 200 response, body echoed back |
| Status codes | 200, 404, 500 — success flag matches |
| Timeouts | 1s delay completes, 5s delay with 2s timeout fails |
| Chunked responses | Streaming endpoint received |
| Large responses | 1MB truncated gracefully, 1MB streaming truncated, 500B fits exactly |
| Edge cases | Invalid domain fails, empty POST body |

`test_ntp_time_provider` — 10 pure host tests, no network: validity before
and after sync, full 64-bit epoch-ms with no truncation, monotonic
arithmetic between refreshes, TTL cache hits/misses, zero-TTL bypass,
recovery after sync loss, and the `ungula::core::time` fallback to local
`millis()` while the provider is invalid.

The ESP-NOW transport, pairing FSMs, connection manager, `MessageHeader`
helpers and `WifiConfigStore` have no tests yet.

## Dependencies

| Library | Repo | Used for |
| ------- | ---- | -------- |
| UngulaCore | [ungula-core](https://github.com/alexconesap/ungula-core.git) | `IPreferences` (NVS), `ungula::core::time` + `ITimeProvider`, `crc32`, string utils |
| embLogX | [emblogx](https://github.com/alexconesap/emblogx.git) | Logging via `log_error()` / `log_warn()`, and the NTP timestamp hook |

ESP-IDF component dependencies (part of the SDK, no extra components needed):

- **esp_wifi** — WiFi AP/STA
- **esp_http_client** — HTTP client
- **esp_http_server** — HTTP+WebSocket server
- **esp_sntp** — NTP time synchronisation

For local development, keep the libraries as siblings:

```text
your_workspace/
  lib/            <- UngulaCore
  lib_emblogx/    <- embLogX
  lib_net/        <- this library
```

## Acknowledgements

Thanks to Claude and ChatGPT for helping on generating this documentation.

## License

MIT License — see [LICENSE](LICENSE) file.

---

## Arduino CLI symlink note (rarely relevant)

This library ships a flat forwarder header at `src/ungula_net.h` that
just `#include`s `ungula/net.h`. `library.properties` `includes=` points
at the forwarder.

It only exists to work around an Arduino CLI quirk: when the library is
consumed through a symlink, the CLI sometimes fails to discover headers
nested under `src/ungula/`. The flat forwarder fixes that scan.

**Host code keeps including the real header**:

```cpp
#include <ungula/net.h>
```

PlatformIO, ESP-IDF component builds, and plain CMake setups can ignore
the forwarder.
