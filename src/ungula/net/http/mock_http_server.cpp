// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Alex Conesa

// =============================================================================
// HttpServer — mock / host stub. Selected by -DUNGULA_NET_MOCK. Every method is
// a no-op reporting "not running", so host builds and tests link without an ESP
// httpd. Wholly guarded so it never collides with a real platform impl.
// =============================================================================
#if defined(UNGULA_NET_MOCK)

#include "http_server.h"

namespace ungula::net::http
{

bool HttpServer::start(uint16_t /*port*/)
{
        return false;
}

void HttpServer::stop()
{
}

void HttpServer::route(Method /*method*/, const char * /*path*/, RouteHandler /*handler*/)
{
}

void HttpServer::setNotFoundHandler(RouteHandler /*handler*/)
{
}

void HttpServer::ready()
{
}

bool HttpServer::enableWebSocket(const char * /*path*/)
{
        return false;
}

int HttpServer::wsBroadcast(const char * /*data*/, size_t /*len*/)
{
        return 0;
}

int HttpServer::wsPing()
{
        return 0;
}

int HttpServer::wsClientCount() const
{
        return 0;
}

bool HttpServer::isRunning() const
{
        return false;
}

} // namespace ungula::net::http

#endif // UNGULA_NET_MOCK
