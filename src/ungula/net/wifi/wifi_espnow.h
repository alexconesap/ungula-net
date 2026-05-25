// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#pragma once

/**
 * @file wifi_espnow.h
 * @brief Minimal WiFi initialization for ESP-NOW communication.
 *
 * Use this header for nodes that only need ESP-NOW (no web server, no AP).
 * It initializes the WiFi radio in STA mode — the minimum required for
 * ESP-NOW to function.
 *
 * @warning **MANDATORY on ESP32 (pure ESP-IDF builds)**: NVS must be
 * initialised BEFORE calling `espnow_init()`. The WiFi driver persists
 * calibration data through NVS and will fail with
 * `ESP_ERR_NVS_NOT_INITIALIZED` (and reboot the chip) otherwise. Either
 * call `ungula::core::preferences::initStorage()` (from `lib`) which
 * handles the erase-and-retry case, or invoke `nvs_flash_init()` from
 * `<nvs_flash.h>` directly.
 *
 * Arduino-ESP32 ran the NVS init implicitly at startup, so Arduino
 * sketches never saw this requirement. ESP-IDF projects do.
 *
 * Usage:
 * @code
 *   #include <ungula/core/preferences/preferences.h>
 *   #include <wifi/wifi_espnow.h>
 *
 *   void setup() {
 *     // REQUIRED on ESP-IDF — must come first.
 *     if (!ungula::core::preferences::initStorage()) {
 *       // handle error
 *     }
 *     if (!ungula::net::wifi::espnow_init()) {
 *       // handle error
 *     }
 *     // ESP-NOW transport is now ready to use
 *   }
 * @endcode
 */

namespace ungula::net::wifi
{

/// Initialize WiFi radio for ESP-NOW communication.
/// Sets up WiFi in STA mode — the minimum required for ESP-NOW.
/// No AP is started, no HTTP server, no web UI.
///
/// @warning REQUIRES NVS to be initialised first on ESP-IDF (see file
/// docblock). Call `ungula::core::preferences::initStorage()` before
/// this function.
///
/// @return true on success
bool espnow_init();

} // namespace ungula::net::wifi
