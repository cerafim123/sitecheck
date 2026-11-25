#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "core.h"

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

#define MAX_URLS 256

static void print_help(void) {
    printf("Usage: sitecheck [OPTIONS] [FILE]\n");
    printf("Check HTTP availability of URLs.\n\n");
    printf("Options:\n");
    printf("  -u URL       Check a single URL (can be used multiple times)\n");
    printf("  -t SECONDS   Request timeout (default: 10)\n");
    printf("  --help       Show this help\n");
    printf("  --version    Show version\n\n");
    printf("If FILE is given, it must contain one URL per line.\n");
    printf("Lines starting with # are ignored.\n");
}

int main(int argc, char *argv[]) {
    const char *urls[MAX_URLS];
    size_t url_count = 0;
    long timeout = 10;
    const char *file = NULL;

    static struct option long_opts[] = {
        {"help",    no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "u:t:h", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'u':
                if (url_count >= MAX_URLS) {
                    fprintf(stderr, "Too many URLs (max %d)\n", MAX_URLS);
                    return 1;
                }
                urls[url_count++] = optarg;
                break;
            case 't':
                timeout = atol(optarg);
                if (timeout <= 0) {
                    fprintf(stderr, "Timeout must be positive\n");
                    return 1;
                }
                break;
            case 'h':
                print_help();
                return 0;
            case 'v':
                printf("sitecheck %s\n", APP_VERSION);
                return 0;
            default:
                return 1;
        }
    }

    if (optind < argc) {
        file = argv[optind];
    }

    char *file_urls[MAX_URLS];
    size_t file_url_count = 0;

    if (file) {
        FILE *f = fopen(file, "r");
        if (!f) {
            perror("fopen");
            return 1;
        }
        char line[1024];
        while (fgets(line, sizeof(line), f) != NULL && file_url_count < MAX_URLS) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] == '\0' || line[0] == '#') continue;
            size_t len = strlen(line) + 1;
            file_urls[file_url_count] = malloc(len);
            if (file_urls[file_url_count]) {
                memcpy(file_urls[file_url_count], line, len);
            }
            if (!file_urls[file_url_count]) {
                fclose(f);
                for (size_t i = 0; i < file_url_count; i++) free(file_urls[i]);
                return 1;
            }
            file_url_count++;
        }
        fclose(f);
    }

    const char *all_urls[MAX_URLS];
    size_t total = 0;
    for (size_t i = 0; i < url_count; i++) all_urls[total++] = urls[i];
    for (size_t i = 0; i < file_url_count; i++) all_urls[total++] = file_urls[i];

    if (total == 0) {
        fprintf(stderr, "No URLs provided. Use -u or specify a file.\n");
        for (size_t i = 0; i < file_url_count; i++) free(file_urls[i]);
        return 1;
    }

    site_result *results = NULL;
    size_t count = 0;
    int rc = check_sites(all_urls, total, timeout, &results, &count);

    if (rc != 0) {
        fprintf(stderr, "Failed to perform checks\n");
        for (size_t i = 0; i < file_url_count; i++) free(file_urls[i]);
        return 1;
    }

    for (size_t i = 0; i < count; i++) {
        if (results[i].success) {
            printf("%s -> HTTP %ld (%.3f s)\n",
                   results[i].url, results[i].http_code, results[i].total_time_sec);
        } else {
            fprintf(stderr, "%s -> ERROR: %s\n",
                    results[i].url, results[i].error_msg);
        }
    }

    check_summary summary;
    build_summary(results, count, &summary);

    printf("\nSummary: %zu checked, %zu OK, %zu failed",
           summary.count, summary.success_count, summary.fail_count);

    if (summary.success_count > 0) {
        printf(", avg time %.3f s",
               summary.total_time_sum / (double)summary.success_count);
    }
    printf("\n");

    free_results(results, count);for (size_t i = 0; i < file_url_count; i++) free(file_urls[i]);

    return (summary.fail_count > 0) ? 1 : 0;
}
