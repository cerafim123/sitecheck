#ifndef SITECHECK_CORE_H
#define SITECHECK_CORE_H

#include <stddef.h>

#define SITECHECK_URL_MAX 2048

typedef struct {
    char url[SITECHECK_URL_MAX];
    long http_code;
    double elapsed_seconds;
    int request_ok;
} SiteResult;

typedef struct {
    size_t total;
    size_t reachable;
    size_t failed;
    double average_seconds;
} SiteSummary;

/* Разбирает строку статуса HTTP, например "HTTP/1.1 200 OK". */
int sitecheck_parse_response(const char response[], size_t response_size,
                             long *http_code);

/* Рассчитывает сводку только по результатам в памяти. */
void sitecheck_summarize(const SiteResult results[], size_t count,
                         SiteSummary *summary);

#endif
