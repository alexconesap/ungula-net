// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

// =============================================================================
// HTTP client — ESP-IDF implementation (esp_http_client). Selected by
// -DESP_PLATFORM; wholly guarded so a new platform adds a sibling
// <platform>_http_client.cpp without touching this file.
// =============================================================================
#if defined(ESP_PLATFORM)

#include "http_client.h"

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>

static const char *TAG = "http_client";

namespace ungula::net::http
{

// Collect response body into HttpResult
static esp_err_t on_data(esp_http_client_event_t *evt)
{
        if (evt->event_id == HTTP_EVENT_ON_DATA) {
                auto *result = static_cast<HttpResult *>(evt->user_data);
                size_t space = sizeof(result->body) - result->bodyLen - 1;
                if (space > 0) {
                        size_t copy = evt->data_len < space ? evt->data_len : space;
                        memcpy(result->body + result->bodyLen, evt->data, copy);
                        result->bodyLen += copy;
                        result->body[result->bodyLen] = '\0';
                }
        }
        return ESP_OK;
}

HttpResult httpGet(const char *url, int timeout_ms)
{
        HttpResult result;

        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = timeout_ms;
        config.event_handler = on_data;
        config.user_data = &result;
        config.crt_bundle_attach = esp_crt_bundle_attach;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == nullptr) {
                ESP_LOGE(TAG, "Failed to init HTTP client for %s", url);
                return result;
        }

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
                result.statusCode = esp_http_client_get_status_code(client);
                result.success = (result.statusCode >= 200 && result.statusCode < 300);
        } else {
                ESP_LOGW(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        return result;
}

HttpResult httpPost(const char *url, const char *json, size_t json_len, int timeout_ms)
{
        HttpResult result;

        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_POST;
        config.timeout_ms = timeout_ms;
        config.event_handler = on_data;
        config.user_data = &result;
        config.crt_bundle_attach = esp_crt_bundle_attach;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == nullptr) {
                ESP_LOGE(TAG, "Failed to init HTTP client for %s", url);
                return result;
        }

        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, json, static_cast<int>(json_len));

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
                result.statusCode = esp_http_client_get_status_code(client);
                result.success = (result.statusCode >= 200 && result.statusCode < 300);
        } else {
                ESP_LOGW(TAG, "POST to %s failed: %s", url, esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        return result;
}

} // namespace ungula::net::http

#endif // ESP_PLATFORM
