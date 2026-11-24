# sitecheck

Вариант 8 — проверка доступности сайтов.

Консольная программа на C11 проверяет список HTTP/HTTPS-адресов, показывает
код ответа и время отклика, затем выводит итоговую сводку.

## Сборка

    sudo apt update
    xargs -a build-deps.txt sudo apt install -y
    cmake -S . -B build
    cmake --build build

## Запуск

    ./build/sitecheck --help
    ./build/sitecheck --version
    ./build/sitecheck check https://example.com

## Практические работы

- ПР 1: программа, Git, CMake и deb-пакет.
- ПР 2: тесты и GitHub Actions.
- ПР 3: HTTP-сервис и Docker.
- ПР 4: развёртывание и сопровождение в Kubernetes.
