# sitecheck

**Вариант 8:** проверка доступности сайтов.

Консольная программа на C11 проверяет HTTP/HTTPS-адреса, выводит код ответа и время отклика, а также итоговую сводку.

## Сборка

Требуемые пакеты перечислены в `build-deps.txt`:

    sudo apt update
    xargs -a build-deps.txt sudo apt install -y
    cmake -S . -B build
    cmake --build build

Показать справку и версию:

    ./build/sitecheck --help
    ./build/sitecheck --version

## Запуск

Проверить список сайтов:

    ./build/sitecheck check https://example.com https://ubuntu.com

Вывести итоговую сводку проверок:

    ./build/sitecheck summary https://example.com https://ubuntu.com

Переопределить тайм-аут в миллисекундах:

    ./build/sitecheck check --timeout-ms 3000 https://example.com

Значение по умолчанию — 5000 мс. При установленном пакете оно читается из `/etc/sitecheck/sitecheck.conf`; параметр `--timeout-ms` имеет приоритет.

Команды возвращают `0` при успешной проверке и ненулевой код при ошибке.

## Сборка и установка deb-пакета

Собрать пакет одной командой:

    ./build_deb.sh

Пакет создаётся в `dist/`. Имя зависит от версии из `VERSION` и архитектуры машины. Установить пакет и его зависимости:

    sudo apt install ./dist/sitecheck_0.1.0_$(dpkg --print-architecture).deb

Проверить установленную программу и зависимости:

    sitecheck --version
    sitecheck check https://example.com
    ldd /usr/bin/sitecheck

Обычное удаление сохраняет конфигурацию:

    sudo apt remove sitecheck

Полное удаление пакета и конфигурации:

    sudo apt purge sitecheck

## Состав практических работ

- **ПР 1:** программа варианта 8, Git-репозиторий, CMake и deb-пакет.
- **ПР 2:** модульные тесты и конвейер GitHub Actions.
- **ПР 3:** HTTP-сервис и Docker-образ.
- **ПР 4:** развёртывание и сопровождение в Kubernetes.

Будущие работы перечислены как план и не считаются выполненными в ПР 1.
