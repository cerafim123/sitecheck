#include "core.h"

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

static void print_help(const char *program)
{
    printf("Использование:\n");
    printf("  %s --help\n", program);
    printf("  %s --version\n", program);
    printf("  %s check [--timeout-ms MS] URL [URL ...]\n", program);
    printf("  %s summary [--timeout-ms MS] URL [URL ...]\n", program);
    printf("\n"
           "check   — проверить адреса и показать результат каждого запроса;\n"
           "summary — проверить адреса и вывести итоговую сводку;\n"
           "MS      — время ожидания в миллисекундах (1–300000).\n");
}

static long read_default_timeout(void)
{
    FILE *file = fopen("/etc/sitecheck/sitecheck.conf", "r");
    char line[256];
    long timeout_ms = DEFAULT_TIMEOUT_MS;

    /* Отсутствие конфигурационного файла не является ошибкой. */
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

static int parse_timeout(const char *text, long *timeout_ms)
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

static void print_summary(const SiteSummary *summary)
{
    printf("Всего сайтов: %zu\n", summary->total);
    printf("Получен HTTP-ответ: %zu\n", summary->reachable);
    printf("Ошибок запроса: %zu\n", summary->failed);
    printf("Среднее время ответа: %.3f с\n", summary->average_seconds);
}

static int run_probe_command(int argc, char **argv, int show_details)
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
                        "Ошибка: --timeout-ms требует целое число от 1 до %ld\n",
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

        if (count >= MAX_SITES) {
            fprintf(stderr, "Ошибка: можно проверить не более %d сайтов\n",
                    MAX_SITES);
            return 2;
        }

        if (strlen(argv[index]) >= sizeof(results[count].url)) {
            fprintf(stderr, "Ошибка: адрес слишком длинный\n");
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
        int probe_status = sitecheck_probe(
            results[site].url,
            timeout_ms,
            &results[site],
            error,
            sizeof(error));

        if (probe_status != 0) {
            failed++;
            fprintf(stderr, "%s: %s\n", results[site].url,
                    error[0] != '\0' ? error : "ошибказапроса");
        }

        if (show_details) {
            printf("%-48s HTTP %ld  %.3f с\n",
                   results[site].url,
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

int main(int argc, char **argv)
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

    if (sitecheck_init() != 0) {
        fprintf(stderr, "Ошибка: не удалось инициализировать libcurl\n");
        return 2;
    }

    result = run_probe_command(
        argc, argv, strcmp(argv[1], "check") == 0);

    sitecheck_cleanup();
    return result;
}
