#ifndef CORE_H
#define CORE_H

#include <stddef.h>

typedef struct {
    char *url;
    long http_code;
    double total_time_sec;
    int success;
    char error_msg[256];
} site_result;

typedef struct {
    site_result *items;
    size_t count;
    size_t success_count;
    size_t fail_count;
    double total_time_sum;
} check_summary;

int check_sites(const char **urls, size_t count, long timeout_sec,
                site_result **out_results, size_t *out_count);

void free_results(site_result *results, size_t count);
void build_summary(const site_result *results, size_t count, check_summary *out);

#endif
