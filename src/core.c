#include "core.h"

#include <curl/curl.h>

#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

static size_t discard_response_body(char *data, size_t size,
                                    size_t count, void *user_data)
{
    (void)data;
    (void)user_data;
    return size * count;
}

int sitecheck_init(void)
{
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK ? 0 : -1;
}

void sitecheck_cleanup(void)
{
    curl_global_cleanup();
}

int sitecheck_probe(const char *url, long timeout_ms, SiteResult *result,
                    char *error, size_t error_size)
{
    CURL *handle;
    CURLcode code;
    CURLcode info_code;
    long http_code = 0;
    double elapsed_seconds = 0.0;
    size_t url_length;
    char url_copy[SITECHECK_URL_MAX];

    if (url == NULL || result == NULL || timeout_ms <= 0) {
        set_error(error, error_size, "Некорректные аргументы проверки");
        return -1;
    }

    url_length = strlen(url);
    if (url_length == 0 || url_length >= sizeof(result->url)) {
        set_error(error, error_size, "Адрес пустой или слишком длинный");
        return -1;
    }

    if (strncmp(url, "http://", 7) != 0 &&
        strncmp(url, "https://", 8) != 0) {
        set_error(error, error_size,
                  "Адрес должен начинаться с http:// или https://");
        return -1;
    }
    (void)memcpy(url_copy, url, url_length+1);
    (void)memset(result, 0, sizeof(*result));
    (void)snprintf(result->url, sizeof(result->url), "%s", url_copy);

    handle = curl_easy_init();
    if (handle == NULL) {
        set_error(error, error_size, "Не удалось создать запрос libcurl");
        return -1;
    }

    code = curl_easy_setopt(handle, CURLOPT_URL, url_copy);
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
        code = curl_easy_setopt(handle, CURLOPT_USERAGENT, "sitecheck/0.1.0");
    }
    if (code == CURLE_OK) {
        code = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION,
                                discard_response_body);
    }

    if (code != CURLE_OK) {
        set_error(error, error_size, curl_easy_strerror(code));
        curl_easy_cleanup(handle);
        return -1;
    }

    code = curl_easy_perform(handle);

    info_code = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
    if (info_code == CURLE_OK) {
        info_code = curl_easy_getinfo(handle, CURLINFO_TOTAL_TIME,
                                      &elapsed_seconds);
    }

    if (info_code == CURLE_OK) {
        result->http_code = http_code;
        result->elapsed_seconds = elapsed_seconds;
    }

    result->request_ok = (code == CURLE_OK &&
                          info_code == CURLE_OK &&
                          http_code > 0) ? 1 : 0;

    if (code != CURLE_OK) {
        set_error(error, error_size, curl_easy_strerror(code));
    } else if (info_code != CURLE_OK) {
        set_error(error, error_size, curl_easy_strerror(info_code));
    } else if (http_code == 0) {
        set_error(error, error_size, "Сервер не вернул HTTP-код");
    }

    curl_easy_cleanup(handle);
    return result->request_ok ? 0 : -1;
}

void sitecheck_summarize(const SiteResult *results, size_t count,
                         SiteSummary *summary)
{
    size_t index;
    double successful_time = 0.0;

    if (summary == NULL) {
        return;
    }

    (void)memset(summary, 0, sizeof(*summary));
    summary->total = count;

    if (count == 0) {
        return;
    }

    if (results == NULL) {
        summary->failed =count;
        return;
    }

    for (index = 0; index < count; index++) {
        if (results[index].request_ok) {
            summary->reachable++;
            successful_time += results[index].elapsed_seconds;
        } else {
            summary->failed++;
        }
    }

    if (summary->reachable > 0) {
        summary->average_seconds =
            successful_time / (double)summary->reachable;
    }
}
