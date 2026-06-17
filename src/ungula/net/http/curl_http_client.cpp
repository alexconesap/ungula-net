// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

// =============================================================================
// HTTP client — desktop / host implementation (libcurl). Selected by
// -DUNGULA_NET_CURL (lib_net's tests + desktop tooling opt in). A real HTTP
// client, not a no-op mock. Wholly guarded so it never collides with an
// on-device platform implementation.
// =============================================================================
#if defined(UNGULA_NET_CURL)

#include "http_client.h"

#include <curl/curl.h>
#include <cstdio>
#include <cstring>

namespace ungula::net::http
{

struct CurlWriteCtx {
        HttpResult *result;
};

static size_t curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
        auto *ctx = static_cast<CurlWriteCtx *>(userdata);
        size_t total = size * nmemb;
        size_t space = sizeof(ctx->result->body) - ctx->result->bodyLen - 1;
        if (space > 0) {
                size_t copy = total < space ? total : space;
                memcpy(ctx->result->body + ctx->result->bodyLen, ptr, copy);
                ctx->result->bodyLen += copy;
                ctx->result->body[ctx->result->bodyLen] = '\0';
        }
        return total;
}

HttpResult httpGet(const char *url, int timeout_ms)
{
        HttpResult result;
        CURL *curl = curl_easy_init();
        if (!curl)
                return result;

        CurlWriteCtx ctx = { &result };
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
                long code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
                result.statusCode = static_cast<int>(code);
                result.success = (code >= 200 && code < 300);
        }

        curl_easy_cleanup(curl);
        return result;
}

HttpResult httpPost(const char *url, const char *json, size_t json_len, int timeout_ms)
{
        HttpResult result;
        CURL *curl = curl_easy_init();
        if (!curl)
                return result;

        CurlWriteCtx ctx = { &result };
        struct curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_len));
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout_ms));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
                long code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
                result.statusCode = static_cast<int>(code);
                result.success = (code >= 200 && code < 300);
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return result;
}

} // namespace ungula::net::http

#endif // UNGULA_NET_CURL
