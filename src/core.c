#include "core.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static size_t discard_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    (void)contents;
    (void)userp;
    return size * nmemb;
}

int check_sites(const char **urls, size_t count, long timeout_sec,
                site_result **out_results, size_t *out_count) {
    if (count == 0) {
        *out_results = NULL;
        *out_count = 0;
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    site_result *results = calloc(count, sizeof(site_result));
    if (!results) {
        curl_easy_cleanup(curl);
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        size_t len = strlen(urls[i]) + 1;
        results[i].url = malloc(len);
        if (results[i].url) {
            memcpy(results[i].url, urls[i], len);
        }
        if (!results[i].url) {
            for (size_t j = 0; j < i; j++) free(results[j].url);
            free(results);
            curl_easy_cleanup(curl);
            return -1;
        }

        curl_easy_setopt(curl, CURLOPT_URL, urls[i]);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discard_cb);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            long code = 0;
            double total = 0.0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total);
            results[i].http_code = code;
            results[i].total_time_sec = total;
            results[i].success = 1;
        } else {
            results[i].success = 0;
            snprintf(results[i].error_msg, sizeof(results[i].error_msg),
                     "%s", curl_easy_strerror(res));
        }
    }

    curl_easy_cleanup(curl);
    *out_results = results;
    *out_count = count;
    return 0;
}

void free_results(site_result *results, size_t count) {
    if (!results) return;
    for (size_t i = 0; i < count; i++) {
        free(results[i].url);
    }
    free(results);
}

void build_summary(const site_result *results, size_t count, check_summary *out) {
    memset(out, 0, sizeof(*out));
    out->items = (site_result *)results;
    out->count = count;

    for (size_t i = 0; i < count; i++) {
        if (results[i].success) {
            out->success_count++;
            out->total_time_sum += results[i].total_time_sec;
        } else {
            out->fail_count++;
        }
    }
}
