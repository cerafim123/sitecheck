# sitecheck

**Вариант 8:** проверка доступности сайтов.

Консольная программа на C11 проверяет HTTP/HTTPS-адреса, показывает HTTP-код,
время отклика и итоговую сводку.

## Сборка

Пакеты для сборки перечислены в `build-deps.txt`:

    sudo apt update
    xargs -a build-deps.txt sudo apt install -y
    cmake -S . -B build
    cmake --build build

## Запуск

    ./build/sitecheck --help
    ./build/sitecheck --version
    ./build/sitecheck check https://example.com https://ubuntu.com
    ./build/sitecheck summary --timeout-ms 3000 https://example.com

По умолчанию тайм-аут равен 5000 мс. Установленная программа читает его из
`/etc/sitecheck/sitecheck.conf`; параметр `--timeout-ms` имеет приоритет.

Код возврата: `0` — все запросы успешны, `1` — ошибка хотя бы одного запроса,
`2` — ошибка аргументов или запуска.

## Сборка и установка deb-пакета

    ./build_deb.sh
    sudo apt install ./dist/sitecheck_0.1.0_$(dpkg --print-architecture).deb

Сценарий собирает пакет в `dist/`; архитектура и runtime-зависимости определяются
автоматически. Проверки пакета:

    dpkg-deb --info dist/sitecheck_0.1.0_$(dpkg --print-architecture).deb
    lintian dist/sitecheck_0.1.0_$(dpkg --print-architecture).deb

Удаление с сохранением конфигурации и полное удаление:

    sudo apt remove sitecheck
    sudo apt purge sitecheck

## Практические работы

- **ПР 1:** программа варианта 8, Git, CMake, man-страница и deb-пакет.
- **ПР 2:** модульные тесты и GitHub Actions.
- **ПР 3:** HTTP-сервис и Docker-образ.
- **ПР 4:** развёртывание и сопровождение в Kubernetes.

ПР 2–4 перечислены как план и не считаются выполненными в ПР 1.
