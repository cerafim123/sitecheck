#include "core.h"

#include <string.h>

int sitecheck_parse_response(const char response[], size_t response_size,
                             long *http_code)
{
    size_t position = 5;
    long status;

    if (response == NULL || http_code == NULL ||
        response_size < 9 || memcmp(response, "HTTP/", 5) != 0) {
        return -1;
    }

    while (position < response_size && response[position] != ' ' &&
           response[position] != '\t' && response[position] != '\r' &&
           response[position] != '\n') {
        position++;
    }

    while (position < response_size &&
           (response[position] == ' ' || response[position] == '\t')) {
        position++;
    }

    if (position + 3 > response_size ||
        response[position] < '0' || response[position] > '9' ||
        response[position + 1] < '0' || response[position + 1] > '9' ||
        response[position + 2] < '0' || response[position + 2] > '9') {
        return -1;
    }

    status = (long)(response[position] - '0') * 100L +
             (long)(response[position + 1] - '0') * 10L +
             (long)(response[position + 2] - '0');

    if (status < 100 || status > 599) {
        return -1;
    }

    if (position + 3 < response_size &&
        response[position + 3] != ' ' &&
        response[position + 3] != '\t' &&
        response[position + 3] != '\r' &&
        response[position + 3] != '\n') {
        return -1;
    }

    *http_code = status;
    return 0;
}

void sitecheck_summarize(const SiteResult results[], size_t count,
                         SiteSummary *summary)
{
    size_t index;
    double successful_time = 0.0;

    if (summary == NULL) {
        return;
    }

    memset(summary, 0, sizeof(*summary));
    summary->total = count;

    if (results == NULL) {
        summary->failed = count;
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
