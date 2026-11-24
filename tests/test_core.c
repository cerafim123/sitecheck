#include "core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef TEST_DATA_DIR
#error "TEST_DATA_DIR must be set by CMake"
#endif

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static int load_fixture(const char name[], char buffer[], size_t capacity,
                        size_t length[])
{
    char path[1024];
    FILE *file;
    size_t count;

    (void)snprintf(path, sizeof(path), "%s/%s", TEST_DATA_DIR, name);
    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }

    count = fread(buffer, 1, capacity, file);
    if (ferror(file) || !feof(file)) {
        (void)fclose(file);
        return -1;
    }

    (void)fclose(file);
    *length = count;
    return 0;
}

static void test_saved_200_fixture(void)
{
    char response[512];
    size_t length = 0;
    long code = 0;

    CHECK(load_fixture("http-200.txt", response, sizeof(response), &length) == 0);
    CHECK(sitecheck_parse_response(response, length, &code) == 0);
    CHECK(code == 200);
}

static void test_saved_404_fixture(void)
{
    char response[512];
    size_t length = 0;
    long code = 0;

    CHECK(load_fixture("http-404.txt", response, sizeof(response), &length) == 0);
    CHECK(sitecheck_parse_response(response, length, &code) == 0);
    CHECK(code == 404);
}

static void test_status_boundaries(void)
{
    long code = 0;
    const char lowest[] = "HTTP/1.1 100 Continue\r\n";
    const char highest[] = "HTTP/1.1 599 Error\r\n";

    CHECK(sitecheck_parse_response(lowest, sizeof(lowest) - 1, &code) == 0);
    CHECK(code == 100);
    CHECK(sitecheck_parse_response(highest, sizeof(highest) - 1, &code) == 0);
    CHECK(code == 599);
}

static void test_invalid_responses(void)
{
    long code = 200;
    const char bad_prefix[] = "NOTHTTP 200 OK";
    const char bad_status[] = "HTTP/1.1 ABC Bad";
    const char too_low[] = "HTTP/1.1 099 Bad";
    const char too_high[] = "HTTP/1.1 600 Bad";

    CHECK(sitecheck_parse_response(NULL, 0, &code) != 0);
    CHECK(sitecheck_parse_response("", 0, &code) != 0);
    CHECK(sitecheck_parse_response(bad_prefix, sizeof(bad_prefix) - 1, &code) != 0);
    CHECK(sitecheck_parse_response(bad_status, sizeof(bad_status) - 1, &code) != 0);
    CHECK(sitecheck_parse_response(too_low, sizeof(too_low) - 1, &code) != 0);
    CHECK(sitecheck_parse_response(too_high, sizeof(too_high) - 1, &code) != 0);
    CHECK(sitecheck_parse_response("HTTP/", 5, &code) != 0);
    CHECK(sitecheck_parse_response("HTTP/1.1 2000 Bad", 17, &code) != 0);
    CHECK(sitecheck_parse_response("HTTP/1.1 200 OK", 15, NULL) != 0);
}

static void test_summary_mixed_results(void)
{
    const SiteResult results[] = {
        {.http_code = 200, .elapsed_seconds = 1.0, .request_ok = 1},
        {.http_code = 500, .elapsed_seconds = 3.0, .request_ok = 1},
        {.request_ok = 0}
    };
    SiteSummary summary;

    sitecheck_summarize(results, 3, &summary);
    CHECK(summary.total == 3);
    CHECK(summary.reachable == 2);
    CHECK(summary.failed == 1);
    CHECK(summary.average_seconds == 2.0);
}

static void test_summary_empty_and_null(void)
{
    SiteSummary summary;

    sitecheck_summarize(NULL, 0, &summary);
    CHECK(summary.total == 0);
    CHECK(summary.reachable == 0);
    CHECK(summary.failed == 0);
    CHECK(summary.average_seconds == 0.0);

    sitecheck_summarize(NULL, 4, &summary);
    CHECK(summary.total == 4);
    CHECK(summary.reachable == 0);
    CHECK(summary.failed == 4);
}


static void demo_intentional_memory_leak(void)
{
    int *leaked = malloc(sizeof *leaked);
    if (leaked != NULL) {
        *leaked = 42;
    }
}

int main(void)
{
    demo_intentional_memory_leak();
    test_saved_200_fixture();
    test_saved_404_fixture();
    test_status_boundaries();
    test_invalid_responses();
    test_summary_mixed_results();
    test_summary_empty_and_null();

    if (failures != 0) {
        fprintf(stderr, "%d assertion(s) failed\n", failures);
        return EXIT_FAILURE;
    }

    puts("All core tests passed");
    return EXIT_SUCCESS;
}
