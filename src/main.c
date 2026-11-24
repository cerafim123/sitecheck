#include "core.h"

#include <curl/curl.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

#define MAX_SITES 256
#define DEFAULT_TIMEOUT_MS 5000L
#define MAX_TIMEOUT_MS 300000L

typedef struct {
    char status_line[256];
    size_t status_length;
} HeaderCapture;

static void set_error(char error[], size_t error_size,
                      const char message[])
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

// cppcheck-suppress constParameterCallback
static size_t capture_status_line(char data[], size_t size, size_t count,
                                  void *user_data)
{
    HeaderCapture *capture = user_data;
    size_t length = size * count;

    if (capture != NULL && length >= 5 && memcmp(data, "HTTP/", 5) == 0) {
        size_t copy_length = length;

        if (copy_length >= sizeof(capture->status_line)) {
            copy_length = sizeof(capture->status_line) - 1;
        }

        memcpy(capture->status_line, data,copy_length);
        capture->status_line[copy_length] = '\0';
        capture->status_length = copy_length;
    }

    return length;
}

static long read_default_timeout(void)
{
    FILE *file = fopen("/etc/sitecheck/sitecheck.conf", "r");
    char line[256];
    long timeout_ms = DEFAULT_TIMEOUT_MS;

    if (file == NULL) {
        return timeout_ms;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        long configured_timeout;

        if (sscanf(line, "timeout_ms=%ld", &configured_timeout) == 1 &&
            configured_timeout > 0 &&
            configured_timeout <= MAX_TIMEOUT_MS) {
            timeout_ms = configured_timeout;
        }
    }

    (void)fclose(file);
    return timeout_ms;
}

static int parse_timeout(const char text[], long *timeout_ms)
{
    char *end = NULL;
    long value;

    if (text == NULL || text[0] == '\0' || timeout_ms == NULL) {
        return -1;
    }

    errno = 0;
    value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value <= 0 || value > MAX_TIMEOUT_MS) {
        return -1;
    }

    *timeout_ms = value;
    return 0;
}

/* Сетевой ввод-вывод находится в main, не в библиотеке core. */
static int probe_url(const char url[], long timeout_ms, SiteResult *result,
                     char error[], size_t error_size)
{
    CURL *handle;
    CURLcode code;
    double elapsed_seconds = 0.0;
    HeaderCapture capture = {{0}, 0};

    memset(result, 0, sizeof(*result));
    (void)snprintf(result->url, sizeof(result->url), "%s", url);

    handle = curl_easy_init();
    if (handle == NULL) {
        set_error(error, error_size, "Не удалось создать запрос libcurl");
        return -1;
    }

    code = curl_easy_setopt(handle, CURLOPT_URL, url);
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 5L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, timeout_ms);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_USERAGENT,
                                "sitecheck/" APP_VERSION);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION,
                                capture_status_line);
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_HEADERDATA, &capture);
    }

    if (code == CURLE_OK) {
        code = curl_easy_perform(handle);
    }

    if (code != CURLE_OK) {
        set_error(error, error_size, curl_easy_strerror(code));
        curl_easy_cleanup(handle);
        return -1;
    }

    code = curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME, &elapsed_seconds);
    if (code != CURLE_OK) {
        set_error(error, error_size, curl_easy_strerror(code));
        curl_easy_cleanup(handle);
        return -1;
    }

    if (sitecheck_parse_response(capture.status_line, capture.status_length,
                                 &result->http_code) != 0) {
        set_error(error, error_size, "Не удалось разобрать HTTP-ответ");
        curl_easy_cleanup(handle);
        return -1;
    }

    result->elapsed_seconds = elapsed_seconds;
    result->request_ok = 1;
    curl_easy_cleanup(handle);
    return 0;
}

static void print_help(const char program[])
{
    printf("Использование:\n");
    printf("  %s --help\n", program);
    printf("  %s --version\n", program);
    printf("  %s check [--timeout-ms MS] URL [URL ...]\n", program);
    printf("  %s summary [--timeout-ms MS] URL [URL ...]\n", program);
}

static void print_summary(const SiteSummary *summary)
{
    printf("Всего сайтов: %zu\n", summary->total);
    printf("Получен HTTP-ответ: %zu\n", summary->reachable);
    printf("Ошибок запроса: %zu\n", summary->failed);
    printf("Среднеевремя ответа: %.3f с\n", summary->average_seconds);
}

static int run_probe_command(int argc, char *argv[], int show_details)
{
    SiteResult results[MAX_SITES];
    SiteSummary summary;
    size_t count = 0;
    long timeout_ms = read_default_timeout();
    int failed = 0;
    int index;

    for (index = 2; index < argc; index++) {
        if (strcmp(argv[index], "--timeout-ms") == 0) {
            if (index + 1 >= argc ||
                parse_timeout(argv[index + 1], &timeout_ms) != 0) {
                fprintf(stderr,
                        "Ошибка: --timeout-ms требует число от 1 до %ld\n",
                        MAX_TIMEOUT_MS);
                return 2;
            }
            index++;
            continue;
        }

        if (argv[index][0] == '-') {
            fprintf(stderr, "Ошибка: неизвестный параметр: %s\n", argv[index]);
            return 2;
        }

        if (count >= MAX_SITES ||
            strlen(argv[index]) >= sizeof(results[count].url)) {
            fprintf(stderr,
                    "Ошибка: слишком много адресов или адрес слишком длинный\n");
            return 2;
        }

        (void)memset(&results[count], 0, sizeof(results[count]));
        (void)snprintf(results[count].url, sizeof(results[count].url),
                       "%s", argv[index]);
        count++;
    }

    if (count == 0) {
        fprintf(stderr, "Ошибка: укажите хотя бы один HTTP/HTTPS-адрес\n");
        return 2;
    }

    for (size_t site = 0; site < count; site++) {
        char error[256] = {0};
        char url[SITECHECK_URL_MAX];

        (void)snprintf(url, sizeof(url), "%s", results[site].url);

        if (strncmp(url, "http://", 7) != 0 &&
            strncmp(url, "https://", 8) != 0) {
            set_error(error, sizeof(error),
                      "Адрес должен начинаться с http:// или https://");
            fprintf(stderr, "%s: %s\n", url, error);
            failed++;
            continue;
        }

        if (probe_url(url, timeout_ms, &results[site],
                      error, sizeof(error)) != 0) {
            fprintf(stderr, "%s: %s\n", url, error);
            failed++;
        }

        if (show_details) {
            printf("%-48s HTTP %ld  %.3f с\n",
                   url,
                   results[site].http_code,
                   results[site].elapsed_seconds);
        }
    }

    sitecheck_summarize(results, count, &summary);
    if (show_details) {
        putchar('\n');
    }
    print_summary(&summary);
    return failed == 0 ? 0 : 1;
}

int main(int argc, char *argv[])
{
    int result;

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", APP_VERSION);
        return 0;
    }

    if (argc < 2) {
        fprintf(stderr, "Ошибка: укажите команду\n");
        print_help(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "check") != 0 &&
        strcmp(argv[1], "summary") != 0) {
        fprintf(stderr, "Ошибка: неизвестная команда: %s\n", argv[1]);
        print_help(argv[0]);
        return 2;
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "Ошибка: не удалось инициализировать libcurl\n");
        return 2;
    }

    result = run_probe_command(argc, argv, strcmp(argv[1], "check") == 0);
    curl_global_cleanup();
    return result;
}
