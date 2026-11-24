#define _POSIX_C_SOURCE 200809L
#include "core.h"

#include <microhttpd.h>
#include <curl/curl.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

#define DEFAULT_PORT 8080U
#define DEFAULT_TIMEOUT_MS 5000L
#define MAX_BODY_SIZE 2048U

typedef struct {
    char body[MAX_BODY_SIZE + 1U];
    size_t used;
} RequestContext;

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static enum MHD_Result queue_text(struct MHD_Connection *connection,
                                  unsigned int status,
                                  const char *content_type,
                                  const char *text)
{
    struct MHD_Response *response;
    enum MHD_Result result;

    response = MHD_create_response_from_buffer(
        strlen(text), (void *)text, MHD_RESPMEM_MUST_COPY);
    if (response == NULL) {
        return MHD_NO;
    }

    (void)MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE,
                                  content_type);
    result = MHD_queue_response(connection, status, response);
    MHD_destroy_response(response);
    return result;
}

static int token_matches(const char *provided, const char *expected)
{
    size_t provided_length;
    size_t expected_length;
    size_t index;
    size_t maximum_length;
    unsigned int difference;

    if (provided == NULL || expected == NULL) {
        return 0;
    }

    provided_length = strlen(provided);
    expected_length = strlen(expected);
    maximum_length = provided_length > expected_length
        ? provided_length : expected_length;
    difference = (unsigned int)(provided_length ^ expected_length);

    for (index = 0; index < maximum_length; index++) {
        unsigned char left = index < provided_length
            ? (unsigned char)provided[index] : 0U;
        unsigned char right = index < expected_length
            ? (unsigned char)expected[index] : 0U;
        difference |= (unsigned int)(left ^ right);
    }

    return difference == 0U;
}

static unsigned int read_port(void)
{
    const char *text = getenv("APP_PORT");
    char *end = NULL;
    unsigned long value;

    if (text == NULL || text[0] == '\0') {
        return DEFAULT_PORT;
    }

    value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value == 0UL || value > 65535UL) {
        return 0U;
    }

    return (unsigned int)value;
}

static long read_timeout(void)
{
    const char *text = getenv("APP_TIMEOUT_MS");
    char *end = NULL;
    long value;

    if (text == NULL || text[0] == '\0') {
        return DEFAULT_TIMEOUT_MS;
    }

    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value <= 0 || value > 300000L) {
        return DEFAULT_TIMEOUT_MS;
    }

    return value;
}

static enum MHD_Result handle_check(struct MHD_Connection *connection,
                                    RequestContext *request)
{
    CURL *curl;
    CURLcode curl_result;
    SiteResult result_data;
    SiteSummary summary;
    char json[256];
    size_t url_length;
    const char *url;
    long http_code = 0L;
    double elapsed = 0.0;
    long timeout = read_timeout();

    url_length = request->used;
    while (url_length > 0U &&
           (request->body[url_length - 1U] == '\n' ||
            request->body[url_length - 1U] == '\r')) {
        url_length--;
    }

    if (url_length == 0U || url_length >= sizeof(request->body) ||
        memchr(request->body, '\0', url_length) != NULL) {
        return queue_text(connection, MHD_HTTP_BAD_REQUEST,
                          "application/json",
                          "{\"error\":\"body must contain one URL\"}\n");
    }

    request->body[url_length] = '\0';
    url = request->body;

    if (strncmp(url, "http://", 7U) != 0 &&
        strncmp(url, "https://", 8U) != 0) {
        return queue_text(connection, MHD_HTTP_BAD_REQUEST,
                          "application/json",
                          "{\"error\":\"URL must use http or https\"}\n");
    }

    curl = curl_easy_init();
    if (curl == NULL) {
        return queue_text(connection, MHD_HTTP_INTERNAL_SERVER_ERROR,
                          "application/json",
                          "{\"error\":\"could not initialize HTTP client\"}\n");
    }

    curl_result = curl_easy_setopt(curl, CURLOPT_URL, url);
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                                       timeout);
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR,
                                       "http,https");
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR,
                                       "http,https");
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_setopt(curl, CURLOPT_USERAGENT,
                                       "sitecheck/" APP_VERSION);
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_perform(curl);
    }

    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                        &http_code);
    }
    if (curl_result == CURLE_OK) {
        curl_result = curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &elapsed);
    }

    if (curl_result != CURLE_OK) {
        curl_easy_cleanup(curl);
        return queue_text(connection, MHD_HTTP_BAD_GATEWAY,
                          "application/json",
                          "{\"error\":\"upstream request failed\"}\n");
    }

    curl_easy_cleanup(curl);

    memset(&result_data, 0, sizeof(result_data));
    result_data.http_code = http_code;
    result_data.elapsed_seconds = elapsed;
    result_data.request_ok = 1;
    sitecheck_summarize(&result_data, 1U, &summary);

    (void)snprintf(json, sizeof(json),
                   "{\"http_code\":%ld,\"elapsed_seconds\":%.6f,"
                   "\"reachable\":%zu,\"failed\":%zu}\n",
                   http_code, elapsed, summary.reachable, summary.failed);

    return queue_text(connection, MHD_HTTP_OK, "application/json", json);
}

static enum MHD_Result request_handler(
    void *closure,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *http_version,
    const char *upload_data,
    size_t *upload_data_size,
    void **connection_context)
{
    RequestContext *request = *connection_context;
    const char *expected_token = closure;
    char version_json[256];
    char hostname[128] = "unknown";

    (void)http_version;

    if (request == NULL) {
        request = calloc(1U, sizeof(*request));
        if (request == NULL) {
            return MHD_NO;
        }
        *connection_context = request;
        return MHD_YES;
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0 &&
        strcmp(url, "/health") == 0) {
        return queue_text(connection, MHD_HTTP_OK, "text/plain",
                          "ok\n");
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0 &&
        strcmp(url, "/ready") == 0) {
        return queue_text(connection, MHD_HTTP_OK, "text/plain",
                          "ready\n");
    }

    if (strcmp(method, MHD_HTTP_METHOD_GET) == 0 &&
        strcmp(url, "/version") == 0) {
        (void)gethostname(hostname,sizeof(hostname) - 1U);
        hostname[sizeof(hostname) - 1U] = '\0';
        (void)snprintf(version_json, sizeof(version_json),
                       "{\"version\":\"%s\",\"host\":\"%s\"}\n",
                       APP_VERSION, hostname);
        return queue_text(connection, MHD_HTTP_OK, "application/json",
                          version_json);
    }

    if (strcmp(url, "/api/check") != 0) {
        return queue_text(connection, MHD_HTTP_NOT_FOUND,
                          "application/json",
                          "{\"error\":\"not found\"}\n");
    }

    if (strcmp(method, MHD_HTTP_METHOD_POST) != 0) {
        return queue_text(connection, MHD_HTTP_METHOD_NOT_ALLOWED,
                          "application/json",
                          "{\"error\":\"POST required\"}\n");
    }

    if (!token_matches(
            MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
                                        "X-Api-Token"),
            expected_token)) {
        return queue_text(connection, MHD_HTTP_UNAUTHORIZED,
                          "application/json",
                          "{\"error\":\"unauthorized\"}\n");
    }

    if (*upload_data_size > 0U) {
        if (*upload_data_size > MAX_BODY_SIZE - request->used) {
            *upload_data_size = 0U;
            return queue_text(connection, 413U, "application/json",
                              "{\"error\":\"request body too large\"}\n");
        }

        memcpy(request->body + request->used, upload_data,
               *upload_data_size);
        request->used += *upload_data_size;
        request->body[request->used] = '\0';
        *upload_data_size = 0U;
        return MHD_YES;
    }

    return handle_check(connection, request);
}

static void request_completed(void *closure,
                              struct MHD_Connection *connection,
                              void **connection_context,
                              enum MHD_RequestTerminationCode termination_code)
{
    (void)closure;
    (void)connection;
    (void)termination_code;

    free(*connection_context);
    *connection_context = NULL;
}

int main(int argc, char *argv[])
{
    const char *api_token;
    unsigned int port;
    struct MHD_Daemon *daemon;
    struct sigaction action;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("Usage: sitecheck-server [--help|--version]");
        puts("Environment: APP_PORT, APP_API_TOKEN, APP_TIMEOUT_MS");
        return EXIT_SUCCESS;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        puts(APP_VERSION);
        return EXIT_SUCCESS;
    }

    if (argc != 1) {
        fprintf(stderr, "Usage: sitecheck-server [--help|--version]\n");
        return 2;
    }

    api_token = getenv("APP_API_TOKEN");
    if (api_token == NULL || api_token[0] == '\0') {
        fprintf(stderr, "APP_API_TOKEN must be set\n");
        return 2;
    }

    port = read_port();
    if (port == 0U) {
        fprintf(stderr, "APP_PORT must be between 1 and 65535\n");
        return 2;
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "Could not initialize libcurl\n");
        return 2;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    (void)sigaction(SIGTERM, &action, NULL);
    (void)sigaction(SIGINT, &action, NULL);

    daemon = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD,
        (uint16_t)port,
        NULL,
        NULL,
        &request_handler,
        (void *)api_token,
        MHD_OPTION_NOTIFY_COMPLETED,
        &request_completed,
        NULL,
        MHD_OPTION_END);

    if (daemon == NULL) {
        fprintf(stderr, "Could not start HTTP server on port %u\n", port);
        curl_global_cleanup();
        return 1;
    }

    printf("sitecheck-server %s listening on port %u\n", APP_VERSION, port);
    fflush(stdout);

    while (stop_requested == 0) {
        (void)sleep(1U);
    }

    MHD_stop_daemon(daemon);
    curl_global_cleanup();
    return 0;
}
