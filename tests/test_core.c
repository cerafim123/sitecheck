#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; } \
    else { fprintf(stderr, "FAIL: %s\n", msg); } \
} while (0)

static void test_empty_summary(void) {
    check_summary s;
    build_summary(NULL, 0, &s);
    CHECK(s.count == 0, "empty summary count");
    CHECK(s.success_count == 0, "empty success");
    CHECK(s.fail_count == 0, "empty fail");
    CHECK(s.success_count == 0, "empty var success");
}

static void test_summary_mixed(void) {
    site_result r[3] = {
        {.url = "a", .success = 1, .total_time_sec = 0.5},
        {.url = "b", .success = 0, .total_time_sec = 0.0},
        {.url = "c", .success = 1, .total_time_sec = 1.5}
    };
    check_summary s;
    build_summary(r, 3, &s);
    CHECK(s.count == 3, "summary count 3");
    CHECK(s.success_count == 2, "summary success 2");
    CHECK(s.fail_count == 1, "summary fail 1");
    CHECK(s.total_time_sum > 1.9 && s.total_time_sum < 2.1, "summary time sum");
    CHECK(s.items == r, "summary items pointer");
}

static void test_free_null(void) {
    free_results(NULL, 0);
    tests_run++;
    tests_passed++;
}

static void test_free_allocated(void) {
    site_result *alloc = malloc(2 * sizeof(site_result));
    memset(alloc, 0, 2 * sizeof(site_result));
    alloc[0].url = malloc(2);
    strcpy(alloc[0].url, "x");
    alloc[1].url = malloc(2);
    strcpy(alloc[1].url, "y");
    free_results(alloc, 2);
    tests_run++;
    tests_passed++;
}

static void test_check_zero(void) {
    site_result *res = NULL;
    size_t cnt = 0;
    int rc = check_sites(NULL, 0, 5, &res, &cnt);
    CHECK(rc == 0, "zero sites returns 0");
    CHECK(cnt == 0, "zero sites count 0");
    free_results(res, cnt);
}

static void test_summary_all_fail(void) {
    site_result r[2] = {0};
    r[0].success = 0;
    r[1].success = 0;
    check_summary s;
    build_summary(r, 2, &s);
    CHECK(s.success_count == 0, "all fail success");
    CHECK(s.fail_count == 2, "all fail fail");
    CHECK(s.total_time_sum == 0.0, "all fail time");
}

static void test_summary_all_ok(void) {
    site_result r[2] = {0};
    r[0].success = 1; r[0].total_time_sec = 1.0;
    r[1].success = 1; r[1].total_time_sec = 3.0;
    check_summary s;
    build_summary(r, 2, &s);
    CHECK(s.success_count == 2, "all ok success");
    CHECK(s.fail_count == 0, "all ok fail");
    CHECK(s.total_time_sum > 3.9 && s.total_time_sum < 4.1, "all ok time");
}

static void test_check_invalid_url(void) {
    const char *urls[1] = { "http://127.0.0.1:1/invalid" };
    site_result *res = NULL;
    size_t cnt = 0;
    int rc = check_sites(urls, 1, 1, &res, &cnt);
    CHECK(rc == 0, "invalid url returns 0");
    CHECK(cnt == 1, "invalid url count 1");
    CHECK(res[0].success == 0, "invalid url not success");
    CHECK(res[0].error_msg[0] != '\0', "invalid url error message");
    free_results(res, cnt);
}

int main(void) {
    test_empty_summary();
    test_summary_mixed();
    test_free_null();
    test_free_allocated();
    test_check_zero();
    test_summary_all_fail();
    test_summary_all_ok();
    test_check_invalid_url();

    printf("Tests: %d/%d passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
