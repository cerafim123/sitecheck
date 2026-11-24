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

/* Инициализация и завершение работы libcurl. */
int sitecheck_init(void);
void sitecheck_cleanup(void);

/*
 * Проверяет один HTTP/HTTPS-адрес.
 * Возвращает 0, если HTTP-обмен завершился и получен код ответа.
 * Возвращает -1 при ошибке аргументов или соединения.
 */
int sitecheck_probe(const char *url, long timeout_ms, SiteResult *result,
                    char *error, size_t error_size);

/* Рассчитывает итоговую сводку по результатам проверок в памяти. */
void sitecheck_summarize(const SiteResult *results, size_t count,
                         SiteSummary *summary);

#endif
