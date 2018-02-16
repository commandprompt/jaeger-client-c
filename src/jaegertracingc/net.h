/*
 * Copyright (c) 2018 Uber Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef JAEGERTRACINGC_NET_H
#define JAEGERTRACINGC_NET_H

#include <assert.h>
#include <http_parser.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "jaegertracingc/alloc.h"
#include "jaegertracingc/common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define JAEGERTRACINGC_MAX_PORT_STR_LEN 6

typedef struct jaeger_host_port {
    char* host;
    int port;
} jaeger_host_port;

#define JAEGERTRACINGC_HOST_PORT_INIT \
    {                                 \
        .host = NULL, .port = 0       \
    }

static inline bool jaeger_host_port_init(jaeger_host_port* host_port,
                                         const char* host,
                                         int port,
                                         jaeger_logger* logger)
{
    assert(host_port != NULL);

    if (host == NULL || strlen(host) == 0) {
        if (logger != NULL) {
            logger->error(logger, "Empty host passed to host port constructor");
        }
        return false;
    }

    if (port < 0 || port > USHRT_MAX) {
        if (logger != NULL) {
            logger->error(
                logger,
                "Invalid port passed to host port constructor, port = %d",
                port);
        }
        return false;
    }

    host_port->host = jaeger_strdup(host, logger);
    if (host_port->host == NULL) {
        if (logger != NULL) {
            logger->error(logger, "Cannot allocate host for host port");
        }
        return false;
    }

    host_port->port = port;
    return true;
}

static inline void jaeger_host_port_destroy(jaeger_host_port* host_port)
{
    assert(host_port != NULL);
    if (host_port->host != NULL) {
        jaeger_free(host_port->host);
        host_port->host = NULL;
    }
}

typedef struct jaeger_url {
    char* str;
    struct http_parser_url parts;
} jaeger_url;

#define JAEGERTRACINGC_URL_INIT  \
    {                            \
        .str = NULL, .parts = {} \
    }

static inline bool
jaeger_url_init(jaeger_url* url, const char* url_str, jaeger_logger* logger)
{
    assert(url != NULL);
    assert(url_str != NULL && strlen(url_str) > 0);
    const int result =
        http_parser_parse_url(url_str, strlen(url_str), 0, &url->parts);
    if (result != 0) {
        if (logger != NULL) {
            logger->error(logger,
                          "Cannot parse URL, URL = \"%s\", error code = %d",
                          url_str,
                          result);
        }
        return false;
    }
    url->str = jaeger_strdup(url_str, logger);
    if (url->str == NULL) {
        if (logger != NULL) {
            logger->error(
                logger, "Cannot allocate URL string, str = \"%s\"", url_str);
        }
        return false;
    }
    return true;
}

static inline void jaeger_url_destroy(jaeger_url* url)
{
    assert(url != NULL);
    if (url->str != NULL) {
        jaeger_free(url->str);
        url->str = NULL;
    }
}

static inline bool jaeger_host_port_from_url(jaeger_host_port* host_port,
                                             const jaeger_url* url,
                                             jaeger_logger* logger)
{
    typedef struct {
        int off;
        int len;
    } str_segment;

    assert(host_port != NULL);
    assert(url != NULL);
    assert(url->str != NULL);
    assert(logger != NULL);

    int port = 0;
    if (url->parts.field_set & (1 << UF_PORT)) {
        const str_segment port_segment = {
            .off = url->parts.field_data[UF_PORT].off,
            .len = url->parts.field_data[UF_PORT].len};
        char* end = NULL;
        port = strtol(&url->str[port_segment.off], &end, 10);
        if (end == NULL ||
            end != &url->str[port_segment.off + port_segment.len]) {
            if (logger != NULL) {
                logger->error(
                    logger, "Cannot parse port, URL = \"%s\"", url->str);
            }
            return false;
        }
    }

    str_segment host_segment = {.off = -1, .len = 0};
    if (url->parts.field_set & (1 << UF_HOST)) {
        host_segment = (str_segment){.off = url->parts.field_data[UF_HOST].off,
                                     .len = url->parts.field_data[UF_HOST].len};
    }

    if (host_segment.len == 0) {
        if (logger != NULL) {
            logger->error(
                logger, "Invalid URL, has no host, URL = \"%s\"", url->str);
        }
        return false;
    }

    char host_buffer[host_segment.len + 1];
    memcpy(&host_buffer[0], &url->str[host_segment.off], host_segment.len);
    host_buffer[host_segment.len] = '\0';
    return jaeger_host_port_init(host_port, &host_buffer[0], port, logger);
}

static inline bool jaeger_host_port_scan(jaeger_host_port* host_port,
                                         const char* str,
                                         jaeger_logger* logger)
{
    assert(host_port != NULL);
    const int len = strlen(str);
    char str_copy[len + 1];
    strncpy(str_copy, str, len);
    str_copy[len] = '\0';
    char* token_context = NULL;
    const char* token = strtok_r(str_copy, ":", &token_context);
    if (token == NULL) {
        if (logger != NULL) {
            logger->error(logger,
                          "Null token for host in host port string, "
                          "host port string = \"%s\"",
                          str);
        }
        return false;
    }
    host_port->host = jaeger_strdup(token, logger);
    if (host_port->host == NULL) {
        return false;
    }

    token = strtok_r(NULL, ":", &token_context);
    if (token == NULL) {
        host_port->port = 0;
        return true;
    }
    char* end = NULL;
    const int port = strtol(token, &end, 10);
    assert(end != NULL);
    if (*end != '\0' || port < 0 || port > USHRT_MAX) {
        if (logger != NULL) {
            logger->error(logger,
                          "Invalid port token in host port string, "
                          "port token = \"%s\", host port string = \"%s\"",
                          token,
                          str);
        }
        jaeger_free(host_port->host);
        host_port->host = NULL;
        return false;
    }
    host_port->port = port;
    return true;
}

static inline int jaeger_host_port_format(const jaeger_host_port* host_port,
                                          char* buffer,
                                          int buffer_len)
{
    assert(host_port != NULL);
    if (host_port->port == 0) {
        return snprintf(buffer, buffer_len, "%s", host_port->host);
    }
    return snprintf(
        buffer, buffer_len, "%s:%d", host_port->host, host_port->port);
}

static inline bool jaeger_host_port_resolve(const jaeger_host_port* host_port,
                                            int socket_type,
                                            struct addrinfo** host_addrs,
                                            jaeger_logger* logger)
{
    assert(host_addrs != NULL);
    assert(host_port != NULL);
    assert(host_port->host != NULL && strlen(host_port->host) > 0);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = socket_type;
    char port_buffer[JAEGERTRACINGC_MAX_PORT_STR_LEN];
    int result =
        snprintf(&port_buffer[0], sizeof(port_buffer), "%d", host_port->port);
    assert(result < JAEGERTRACINGC_MAX_PORT_STR_LEN);
    result = getaddrinfo(host_port->host, &port_buffer[0], &hints, host_addrs);
    if (result != 0) {
        if (logger != NULL) {
            logger->error(logger,
                          "Cannot resolve host = \"%s\", error = \"%s\"",
                          host_port->host,
                          gai_strerror(result));
        }
        return false;
    }
    return true;
}

#ifdef __cplusplus
} /* extern C */
#endif /* __cplusplus */

#endif /* JAEGERTRACINGC_NET_H */
