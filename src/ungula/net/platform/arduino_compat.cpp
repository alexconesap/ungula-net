// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa
// See LICENSE file for details.

// lwIP weak-symbol stub for `lwip_hook_ip6_input`.
//
// Required under:
//   - Arduino-ESP32: Arduino's precompiled liblwip.a expects this hook,
//     usually provided by Arduino's WiFi library; we replace it.
//   - Pure ESP-IDF builds: when CONFIG_LWIP_HOOK_IP6_INPUT_CUSTOM (or
//     equivalent) is enabled, liblwip.a references the symbol and the
//     link fails with `undefined reference to lwip_hook_ip6_input`.
//
// The stub is marked `weak` so any host project providing a real hook
// (e.g. for custom multicast routing) overrides this no-op cleanly.

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)

extern "C" {
struct pbuf;
struct netif;
}

extern "C" __attribute__((weak)) int lwip_hook_ip6_input(struct pbuf *p, struct netif *inp)
{
        (void)p;
        (void)inp;
        return 0;
}

#endif