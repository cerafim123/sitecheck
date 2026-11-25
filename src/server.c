#define _POSIX_C_SOURCE 200809L
#include <microhttpd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core.h"

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

#define DEFAULT_PORT 8080
#define BODY_MAX 4096

static volatile sig_atomic_t keep_running = 1;

struct request_context {
    char body[BODY_MAX];
    size_t length;
    int too_large;
};

static void handle_signal(int sig) {
    (void)sig;
    keep_running = 0;
}

static int json_escape(const char *src, char *dst, size_t cap) {
    size_t used = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p; ++p) {
        const char *escape = NULL;
        char unicode[7];

        if (*p == '"' || *p == '\\') {
            unicode[0] = '\\';
            unicode[1] = (char)*p;
            unicode[2] = '\0';
            escape = unicode;
        } else if (*p < 0x20) {
            (void)snprintf(unicode, sizeof(unicode), "\\u%04x", *p);
            escape = unicode;
        }

        if (escape) {
            size_t n = strlen(escape);
            if (n >= cap - used) return -1;
            memcpy(dst + used, escape, n);
            used += n;
        } else {
            if (used + 1 >= cap) return -1;
            dst[used++] = (char)*p;
        }
    }
    dst[used] = '\0';
    return 0;
}

static enum MHD_Result send_json(struct MHD_Connection *conn,
                                 unsigned int status, const char *json) {
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json), (void *)json, MHD_RESPMEM_MUST_COPY);
    if (!response) return MHD_NO;

    (void)MHD_add_response_header(response, "Content-Type", "application/json");
    enum MHD_Result result = MHD_queue_response(conn, status, response);
    MHD_destroy_response(response);
    return result;
}

static int token_is_valid(struct MHD_Connection *conn) {
    const char *expected = getenv("APP_API_TOKEN");
    const char *provided = MHD_lookup_connection_value(
        conn, MHD_HEADER_KIND, "X-Api-Token");
    return expected && expected[0] && provided &&
           strcmp(expected, provided) == 0;
}

static enum MHD_Result handle_check(struct MHD_Connection *conn,
                                    struct request_context *ctx) {
    while (ctx->length > 0 &&
           (ctx->body[ctx->length - 1] == '\n' ||
            ctx->body[ctx->length - 1] == '\r')) {
        ctx->body[--ctx->length] = '\0';
    }

    if (ctx->too_large)
        return send_json(conn, MHD_HTTP_CONTENT_TOO_LARGE,
                         "{\"error\":\"request body too large\"}");
    if (ctx->length == 0 || strchr(ctx->body, '\n') || strchr(ctx->body, '\r'))
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"send exactly one URL in the body\"}");

    const char *urls[] = { ctx->body };
    site_result *results = NULL;
    size_t count = 0;
    int rc = check_sites(urls, 1, 10, &results, &count);
    if (rc != 0 || count != 1) {
        free_results(results, count);
        return send_json(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                         "{\"error\":\"site check failed\"}");
    }

    char escaped_url[BODY_MAX * 2];
    char escaped_error[512];
    char json[BODY_MAX * 2 + 1024];

    if (json_escape(results[0].url, escaped_url, sizeof(escaped_url)) != 0 ||
        json_escape(results[0].error_msg, escaped_error,
                    sizeof(escaped_error)) != 0) {
        free_results(results, count);
        return send_json(conn, MHD_HTTP_INTERNAL_SERVER_ERROR,
                         "{\"error\":\"response formatting failed\"}");
    }

    if (results[0].success) {
        (void)snprintf(json, sizeof(json),
                       "{\"url\":\"%s\",\"http_code\":%ld,"
                       "\"time_sec\":%.3f,\"ok\":true}",
                       escaped_url, results[0].http_code,
                       results[0].total_time_sec);
    } else {
        (void)snprintf(json, sizeof(json),"{\"url\":\"%s\",\"ok\":false,\"error\":\"%s\"}",
                       escaped_url, escaped_error);
    }

    free_results(results, count);
    return send_json(conn, MHD_HTTP_OK, json);
}

static enum MHD_Result request_handler(
    void *closure, struct MHD_Connection *conn, const char *url,
    const char *method, const char *version, const char *upload_data,
    size_t *upload_data_size, void **con_cls) {
    (void)closure;
    (void)version;

    struct request_context *ctx = *con_cls;
    if (!ctx) {
        ctx = calloc(1, sizeof(*ctx));
        if (!ctx) return MHD_NO;
        *con_cls = ctx;

        if (strcmp(method, "POST") == 0 &&
            strcmp(url, "/api/check") == 0 && !token_is_valid(conn)) {
            return send_json(conn, MHD_HTTP_UNAUTHORIZED,
                             "{\"error\":\"invalid or missing API token\"}");
        }
        return MHD_YES;
    }

    if (*upload_data_size > 0) {
        if (*upload_data_size > BODY_MAX - 1 - ctx->length) {
            ctx->too_large = 1;
        } else if (!ctx->too_large) {
            memcpy(ctx->body + ctx->length, upload_data, *upload_data_size);
            ctx->length += *upload_data_size;
            ctx->body[ctx->length] = '\0';
        }
        *upload_data_size = 0;
        return MHD_YES;
    }

    if (strcmp(method, "GET") == 0 && strcmp(url, "/health") == 0)
        return send_json(conn, MHD_HTTP_OK, "{\"status\":\"ok\"}");

    if (strcmp(method, "GET") == 0 && strcmp(url, "/ready") == 0)
        return send_json(conn, MHD_HTTP_OK, "{\"status\":\"ready\"}");

    if (strcmp(method, "GET") == 0 && strcmp(url, "/version") == 0) {
        char hostname[128] = "unknown";
        char escaped_host[256];
        char json[512];

        if (gethostname(hostname, sizeof(hostname) - 1) != 0)
            (void)snprintf(hostname, sizeof(hostname), "unknown");
        hostname[sizeof(hostname) - 1] = '\0';

        if (json_escape(hostname, escaped_host, sizeof(escaped_host)) != 0)
            (void)snprintf(escaped_host, sizeof(escaped_host), "unknown");

        (void)snprintf(json, sizeof(json),
                       "{\"version\":\"%s\",\"hostname\":\"%s\"}",
                       APP_VERSION, escaped_host);
        return send_json(conn, MHD_HTTP_OK, json);
    }

    if (strcmp(method, "POST") == 0 && strcmp(url, "/api/check") == 0) {
        if (!token_is_valid(conn))
            return send_json(conn, MHD_HTTP_UNAUTHORIZED,
                             "{\"error\":\"invalid or missing API token\"}");
        if (!getenv("APP_API_TOKEN") || !getenv("APP_API_TOKEN")[0])
            return send_json(conn, MHD_HTTP_SERVICE_UNAVAILABLE,
                             "{\"error\":\"API token is not configured\"}");
        return handle_check(conn, ctx);
    }

    if (strcmp(method, "POST") == 0)
        return send_json(conn, MHD_HTTP_NOT_FOUND,
                         "{\"error\":\"not found\"}");

    if (strcmp(method, "GET") != 0)
        return send_json(conn, MHD_HTTP_METHOD_NOT_ALLOWED,
                         "{\"error\":\"method not allowed\"}");

    return send_json(conn, MHD_HTTP_NOT_FOUND, "{\"error\":\"not found\"}");
}

static void request_completed(void *closure, struct MHD_Connection *conn,
                              void **con_cls,
                              enum MHD_RequestTerminationCode toe) {
    (void)closure;
    (void)conn;
    (void)toe;
    free(*con_cls);
    *con_cls = NULL;
}

int main(void) {
    const char *port_env = getenv("APP_PORT");
    char *end = NULL;
    long port = port_env ? strtol(port_env, &end, 10) : DEFAULT_PORT;

    if (port_env && (!end || *end != '\0' || port < 1 || port > 65535)) {
        fprintf(stderr, "Invalid APP_PORT\n");
        return 1;
    }

    (void)signal(SIGINT, handle_signal);
    (void)signal(SIGTERM, handle_signal);

    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD, (uint16_t)port,
        NULL, NULL, &request_handler, NULL,
        MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL,
        MHD_OPTION_END);if (!daemon) {
        fprintf(stderr, "Failed to start HTTP daemon on port %ld\n", port);
        return 1;
    }

    printf("sitecheck-server %s listening on port %ld\n", APP_VERSION, port);
    fflush(stdout);

    while (keep_running) sleep(1);

    MHD_stop_daemon(daemon);
    return 0;
}
