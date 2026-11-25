#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include "core.h"

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

#define DEFAULT_PORT 8080

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    keep_running = 0;
}

static enum MHD_Result send_json(struct MHD_Connection *conn,
                                 unsigned int status,
                                 const char *json) {
    struct MHD_Response *resp = MHD_create_response_from_buffer(
        strlen(json), (void *)json, MHD_RESPMEM_MUST_COPY);
    if (!resp) return MHD_NO;
    MHD_add_response_header(resp, "Content-Type", "application/json");
    int ret = MHD_queue_response(conn, status, resp);
    MHD_destroy_response(resp);
    return (enum MHD_Result)ret;
}

static enum MHD_Result handle_version(struct MHD_Connection *conn) {
    char version_json[256];
    snprintf(version_json, sizeof(version_json),
             "{\"version\":\"%s\"}", APP_VERSION);
    return send_json(conn, MHD_HTTP_OK, version_json);
}

static enum MHD_Result handle_hostname(struct MHD_Connection *conn) {
    char hostname[128] = "unknown";
    if (gethostname(hostname, sizeof(hostname) - 1) != 0) {
        snprintf(hostname, sizeof(hostname), "unknown");
    }
    hostname[sizeof(hostname) - 1] = '\0';

    char json[256];
    snprintf(json, sizeof(json), "{\"hostname\":\"%s\"}", hostname);
    return send_json(conn, MHD_HTTP_OK, json);
}

static enum MHD_Result handle_check(struct MHD_Connection *conn,
                                    const char *target) {
    const char *urls[1] = { target };
    site_result *results = NULL;
    size_t count = 0;
    int rc = check_sites(urls, 1, 10, &results, &count);

    char json[2048];
    if (rc == 0 && count > 0 && results[0].success) {
        snprintf(json, sizeof(json),
                 "{\"url\":\"%s\",\"http_code\":%ld,\"time\":%.3f,\"ok\":true}",
                 results[0].url,
                 results[0].http_code,
                 results[0].total_time_sec);
    } else {
        const char *err = (count > 0) ? results[0].error_msg : "internal error";
        snprintf(json, sizeof(json),
                 "{\"url\":\"%s\",\"ok\":false,\"error\":\"%s\"}",
                 target, err);
    }

    free_results(results, count);
    return send_json(conn, MHD_HTTP_OK, json);
}

static enum MHD_Result request_handler(void @id335025344 (*closure),
                                       struct MHD_Connection *conn,
                                       const char *url,
                                       const char *method,
                                       const char *version,
                                       const char *upload_data,
                                       size_t *upload_data_size,
                                       void **con_cls) {
    (void)closure;
    (void)version;
    (void)upload_data;
    (void)upload_data_size;
    (void)con_cls;

    if (strcmp(method, "GET") != 0) {
        return send_json(conn, MHD_HTTP_METHOD_NOT_ALLOWED,
                         "{\"error\":\"method not allowed\"}");
    }

    if (strcmp(url, "/version") == 0) {
        return handle_version(conn);
    }

    if (strcmp(url, "/hostname") == 0) {
        return handle_hostname(conn);
    }

    const char *target = MHD_lookup_connection_value(
        conn, MHD_GET_ARGUMENT_KIND, "url");

    if (!target || target[0] == '\0') {
        return send_json(conn, MHD_HTTP_BAD_REQUEST,
                         "{\"error\":\"missing url parameter\"}");
    }

    return handle_check(conn, target);
}

int main(void) {
    const char *port_env = getenv("PORT");
    int port = port_env ? atoi(port_env) : DEFAULT_PORT;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        (uint16_t)port,
        NULL, NULL,
        &request_handler, NULL,MHD_OPTION_END);

    if (!daemon) {
        fprintf(stderr, "Failed to start HTTP daemon on port %d\n", port);
        return 1;
    }

    printf("sitecheck-server %s listening on 0.0.0.0:%d\n",
           APP_VERSION, port);
    fflush(stdout);

    while (keep_running) {
        sleep(1);
    }

    MHD_stop_daemon(daemon);
    printf("sitecheck-server stopped\n");
    return 0;
}
