#include <stdio.h>
#include <string.h>

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

static void print_help(const char *program)
{
    printf("Использование:\n");
    printf("  %s --help\n", program);
    printf("  %s --version\n", program);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", APP_VERSION);
        return 0;
    }

    fprintf(stderr, "Ошибка: пока доступна только справка и версия.\n");
    print_help(argv[0]);
    return 2;
}
