# Sitecheck

**Вариант 8.** Консольная программа и HTTP-сервис для проверки доступности сайтов. Программа выводит HTTP-код ответа, время отклика и итоговую сводку.

## Возможности

- Проверка одного или нескольких URL.
- Чтение URL из файла — по одному адресу на строку; строки, начинающиеся с `#`, игнорируются.
- Настройка тайм-аута запросов.
- Вывод версии.
- HTTP-сервис с проверками `/health`, `/ready`, `/version` и защищённым `POST /api/check`.

Версия хранится в файле `VERSION`.

## Зависимости для сборки

Список пакетов находится в `build-deps.txt`. На Ubuntu 24.04 установить зависимости и собрать программу можно так:

```bash
sudo apt-get update
xargs -a build-deps.txt sudo apt-get install -y

cmake -S . -B build
cmake --build build
```

Запустить тесты:

```bash
ctest --test-dir build --output-on-failure
```

## Использование CLI

Проверить один сайт:

```bash
./build/sitecheck -u https://example.com
```

Проверить несколько сайтов и задать тайм-аут:

```bash
./build/sitecheck -t 5 \
  -u https://example.com \
  -u https://example.org
```

Прочитать адреса из файла:

```bash
./build/sitecheck sites.txt
```

Показать справку и версию:

```bash
./build/sitecheck --help
./build/sitecheck --version
```

## Сборка и установка deb-пакета

Собрать пакет:

```bash
./build_deb.sh
```

Пакет появится в `dist/`. Установить его:

```bash
sudo apt install ./dist/sitecheck_0.3.0_amd64.deb
```

Удалить программу, сохранив конфигурацию:

```bash
sudo apt remove sitecheck
```

Удалить также конфигурацию:

```bash
sudo apt purge sitecheck
```

## HTTP API и Docker

Сервис слушает порт из переменной `APP_PORT` (по умолчанию `8080`). Для `POST /api/check` требуется переменная `APP_API_TOKEN`; передавайте её значение в заголовке `X-Api-Token`. Тело запроса — один URL обычным текстом.

Пример запуска контейнера с уже опубликованным образом:

```bash
docker run -d --name sitecheck \
  -p 8080:8080 \
  -e APP_PORT=8080 \
  -e APP_API_TOKEN='замените-на-свой-токен' \
  cerafim123/sitecheck:0.3.0
```

Проверить состояние сервиса и версию:

```bash
curl -fsS http://localhost:8080/health
curl -fsS http://localhost:8080/ready
curl -fsS http://localhost:8080/version
```

Проверить URL через API:

```bash
curl -fsS -X POST \
  -H 'X-Api-Token: замените-на-свой-токен' \
  --data-binary 'https://example.com' \
  http://localhost:8080/api/check
```

Остановить и удалить контейнер:

```bash
docker stop sitecheck
docker rm sitecheck
```

### Размер Docker-образа

Запуск CI прошёл, но фактический размер образа в его логе не найден. Не подставляйте оценочное значение. Получите размер опубликованного образа:

```bash
docker pull cerafim123/sitecheck:0.3.0
docker image inspect cerafim123/sitecheck:0.3.0 \
  --format='{{.Size}}'
```

Переведите выведенное число байт в MiB:

```bash
python3 -c 'print(f"{int(input()) / 1048576:.1f} MiB")'
```

Укажите полученное значение здесь после проверки: **размер образа — заменить фактическим значением**.

## CI и выпуск

GitHub Actions последовательно выполняет сборку, тесты и пакетирование. Для тега версии запускается deploy: CI получает `.deb`-артефакт, собирает Docker-образ, проверяет его smoke-тестом и публикует в Docker Hub с тегом версии и коротким хешем коммита.

Проверенная версия ПР 3 отмечена тегом `v0.3.0`. GitHub Release содержит deb-пакет.

## Практические работы

- **ПР 1:** программа на C, CMake и сборка deb-пакета.
- **ПР 2:** CI, CTest, санитайзеры, проверка покрытия, cppcheck и пакетирование.
- **ПР 3:** HTTP-сервис, Docker-образ из deb-артефакта, smoke-тест и публикация образа.
- **ПР 4:** развёртывание и сопровождение приложения в Kubernetes — выполняется отдельно.
