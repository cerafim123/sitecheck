# sitecheck

**Вариант 8 — проверка доступности сайтов.**

`sitecheck` — консольная программа на C11 для проверки доступности сайтов. Она проверяет HTTP/HTTPS-адреса, показывает HTTP-код и время отклика, затем выводит итоговую сводку.

## Зависимости

Список пакетов для сборки хранится в `build-deps.txt`:

```bash
sudo apt update
xargs -a build-deps.txt sudo apt install -y
```

## Сборка

```bash
cmake -S . -B build
cmake --build build
```

## Запуск

```bash
./build/sitecheck --help
./build/sitecheck --version
./build/sitecheck check https://example.com https://ubuntu.com
./build/sitecheck summary --timeout-ms 3000 https://example.com
```

Тайм-аут по умолчанию — 5000 мс. Установленная программа читает его из `/etc/sitecheck/sitecheck.conf`; аргумент `--timeout-ms` имеет приоритет. Отсутствие файла конфигурации не является ошибкой.

Код возврата: `0` — все запросы успешны, `1` — ошибка хотя бы одного запроса, `2` — ошибка аргументов или запуска.

## Сборка и установка deb-пакета

Собрать пакет:

```bash
./build_deb.sh
```

Пакет создаётся в `dist/`. Версия берётся из `VERSION`, архитектура и runtime-зависимости определяются автоматически.

Установить собранную версию:

```bash
VERSION="$(cat VERSION)"
ARCH="$(dpkg --print-architecture)"
sudo apt install "./dist/sitecheck_${VERSION}_${ARCH}.deb"
```

Проверить пакет:

```bash
VERSION="$(cat VERSION)"
ARCH="$(dpkg --print-architecture)"
dpkg-deb --info "./dist/sitecheck_${VERSION}_${ARCH}.deb"
dpkg-deb --contents "./dist/sitecheck_${VERSION}_${ARCH}.deb"
lintian --fail-on error "./dist/sitecheck_${VERSION}_${ARCH}.deb"
```

Удаление с сохранением конфигурации и полное удаление:

```bash
sudo apt remove sitecheck
sudo apt purge sitecheck
```

## Тесты

Тесты используют сохранённые HTTP-ответы из `tests/data/` и не обращаются к сети. Сборка тестов с AddressSanitizer, UndefinedBehaviorSanitizer и покрытием:

```bash
cmake -S . -B build-test \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="--coverage -fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage -fsanitize=address,undefined"

cmake --build build-test

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  ctest --test-dir build-test --output-on-failure \
  --output-junit build-test/junit.xml

gcovr --root . --filter 'src/core\.c$' \
  --fail-under-line 70 build-test
```

Статический анализ:

```bash
cppcheck --enable=warning,style,performance \
  --suppress=constParameterCallback:src/main.c \
  --error-exitcode=1 -q src
```

## Непрерывная интеграция

GitHub Actions запускает последовательные этапы `build → test → package` при отправке изменений и создании pull request в `main`:

1. **build** — сборка с `-Werror` и проверка `cppcheck`;
2. **test** — тесты с санитайзерами, отчёт JUnit и проверка покрытия `core`;
3. **package** — сборка deb-пакета и проверка `lintian`.

Отчёт тестов и собранный пакет сохраняются как артефакты workflow. Для `main` настроена защита: требуется pull request и успешные проверки перед слиянием. В отдельной демонстрационной ветке намеренная утечка памяти приводит к провалу тестового этапа; эта ветка не вливается в `main`.

## Практические работы

- **ПР 1 — выполнена:** программа варианта 8, Git-репозиторий, модуль `core`, CMake-сборка, конфигурация, man-страница и deb-пакет. Тег: `v0.1.0`.
- **ПР 2 — выполнена:** модульные тесты, CTest, ASan/UBSan, проверка покрытия, `cppcheck`, последовательный CI и выпуск deb-пакета. Тег: `v0.2.0`.
- **ПР 3 — не выполнена:** HTTP-сервис на libmicrohttpd, Docker-образ и его публикация.
- **ПР 4 — не выполнена:** развёртывание и сопровождение приложения в Kubernetes.

## Версии

- `v0.1.0` — ПР 1.
- `v0.2.0` — ПР 2.
- `v0.3.0` — планируемый выпуск ПР 3.
- `v1.0.0` — планируемый выпуск ПР 4.
EOF
```

После выполнения проверьте, что файл изменился:

```bash
git diff --check
git diff -- README.md
```
