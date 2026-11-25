# Sitecheck

**Вариант 8 — проверка доступности сайтов.**

`sitecheck` — консольная программа и HTTP-сервис для проверки доступности сайтов. Программа выводит HTTP-код ответа, время отклика и итоговую сводку.

## Возможности

- Проверка одного или нескольких URL.
- Чтение URL из файла.
- Настройка тайм-аута запросов.
- Вывод справки и версии.
- HTTP-маршруты `/health`, `/ready`, `/version`.
- Защищённый токеном API `POST /api/check`.
- Сборка `.deb`-пакета.
- CI-сборка, тестирование, пакетирование и публикация Docker-образа.
- Развёртывание приложения в Kubernetes с двумя репликами.

Версия хранится в файле `VERSION` и используется при сборке программы, пакета и Docker-образа.

## Сборка и тесты

Список пакетов, необходимых для сборки, находится в `build-deps.txt`. На Ubuntu установить зависимости и собрать программу можно так:

```bash
sudo apt-get update
xargs -a build-deps.txt sudo apt-get install -y

cmake -S . -B build
cmake --build build
```

Запуск тестов:

```bash
ctest --test-dir build --output-on-failure
```

Справка и версия:

```bash
./build/sitecheck --help
./build/sitecheck --version
```

## Использование CLI

Проверить один сайт:

```bash
./build/sitecheck -u https://example.com
```

Проверить несколько сайтов с тайм-аутом пять секунд:

```bash
./build/sitecheck -t 5 \
  -u https://example.com \
  -u https://example.org
```

Прочитать список адресов из файла:

```bash
./build/sitecheck sites.txt
```

В файле указывается один URL на строку. Строки, начинающиеся с `#`, игнорируются.

## Сборка и установка deb-пакета

Собрать пакет:

```bash
./build_deb.sh
```

Готовый `.deb` появится в каталоге `dist/`. Установить пакет:

```bash
sudo apt install ./dist/sitecheck_*.deb
```

Проверить установленные файлы:

```bash
dpkg -L sitecheck
```

Удалить программу, сохранив конфигурацию:

```bash
sudo apt remove sitecheck
```

Удалить программу вместе с конфигурацией:

```bash
sudo apt purge sitecheck
```

## HTTP API

Сервер использует порт `8080` по умолчанию. Его можно изменить переменной `APP_PORT`. Запрос к `POST /api/check` должен содержать URL в виде обычного текста. Для авторизации нужен токен `APP_API_TOKEN`, передаваемый в заголовке `X-Api-Token`.

Запустить сервер из каталога сборки:

```bash
APP_PORT=8080 APP_API_TOKEN='replace-with-your-token' \
  ./build/sitecheck-server
```

Проверить состояние и версию сервиса:

```bash
curl -fsS http://localhost:8080/health
curl -fsS http://localhost:8080/ready
curl -fsS http://localhost:8080/version
```

Проверить URL через API:

```bash
curl -fsS -X POST \
  -H 'X-Api-Token: replace-with-your-token' \
  --data-binary 'https://example.com/' \
  http://localhost:8080/api/check
```

Не записывайте реальные токены в репозиторий, команды с секретами или публичные логи.

## Docker

Образ проекта публикуется в Docker Hub под именем:

```text
cerafim125/sitecheck
```

Запустить опубликованный образ версии `0.3.0`:

```bash
docker run -d --name sitecheck \
  -p 8080:8080 \
  -e APP_PORT=8080 \
  -e APP_API_TOKEN='replace-with-your-token' \
  cerafim125/sitecheck:0.3.0
```

Проверить контейнер и сервис:

```bash
docker ps
docker logs sitecheck
curl -fsS http://localhost:8080/health
curl -fsS http://localhost:8080/version
```

Остановить и удалить контейнер:

```bash
docker stop sitecheck
docker rm sitecheck
```

### Размер Docker-образа

Измерить размер образа версии `0.3.0`:

```bash
docker pull cerafim125/sitecheck:0.3.0

docker image inspect cerafim125/sitecheck:0.3.0 \
  --format='{{.Size}}' |
  python3 -c 'import sys; print(f"{int(sys.stdin.read()) / 1048576:.2f} MiB")'
```

В README нужно указать результат этой команды после измерения. Размер `.deb`-пакета не является размером Docker-образа.

## CI и выпуск версий

GitHub Actions выполняет сборку, тестирование и пакетирование. Этапы зависят друг от друга: при ошибке предыдущего этапа следующий не запускается. Для тега версии запускается deploy: CI получает собранный `.deb`, создаёт Docker-образ, запускает контейнер и выполняет smoke-тесты. Образ публикуется только после успешных проверок.

Образ публикуется с тегом версии и коротким SHA коммита. Для входа в Docker Hub в настройках GitHub Actions используются секреты:

- `DOCKERHUB_USERNAME` — имя пользователя Docker Hub;
- `DOCKERHUB_TOKEN` — токен Docker Hub.

Значение `VERSION` должно совпадать с версией в теге без префикса `v`: например, `VERSION=1.0.0` соответствует тегу `v1.0.0`.

Порядок выпуска:

1. Установить версию в `VERSION`.
2. Проверить изменения и пройти CI в pull request.
3. Влить pull request в `main`.
4. Создать и отправить аннотированный тег:

```bash
git switch main
git pull --ff-only origin main
git tag -a v1.0.0 -m "Выпуск версии 1.0.0"
git push origin v1.0.0
```

5. Проверить успешный запуск GitHub Actions, публикацию образа в Docker Hub и создание GitHub Release с `.deb`-пакетом.

Тег выпускайте только после того, как соответствующая версия находится в `main`. Не перемещайте существующий тег, не проверив его назначение и статус релиза.

## Kubernetes: установка и запуск

Манифесты Kubernetes находятся в каталоге `k8s/`:

- `namespace.yaml` — Namespace приложения;
- `configmap.yaml` — настройки сервиса;
- `deployment.yaml` — Deployment с двумя репликами;
- `service.yaml` — Service типа `ClusterIP`.

Для локального запуска нужны Docker Engine, `kubectl` и кластер kind или minikube. Создать кластер kind:

```bash
kind create cluster --name tsp
kubectl get nodes
```

Продолжайте после того, как узел кластера будет в состоянии `Ready`.

Создать Namespace:

```bash
kubectl apply -f k8s/namespace.yaml
```

Создать токен и Secret. Файл с токеном сохраняется в домашнем каталоге вне репозитория:

```bash
umask 077
python3 -c 'import secrets; print(secrets.token_hex(32))' \
  > ~/.sitecheck-api-token

TOKEN="$(cat ~/.sitecheck-api-token)"
kubectl -n sitecheck create secret generic sitecheck-secret \
  --from-literal=APP_API_TOKEN="$TOKEN" \
  --dry-run=client -o yaml |
  kubectl apply -f -
unset TOKEN
```

Убедитесь, что в `k8s/deployment.yaml` указан реально опубликованный тег образа. Применить остальные манифесты:

```bash
kubectl apply -f k8s/configmap.yaml
kubectl apply -f k8s/deployment.yaml
kubectl apply -f k8s/service.yaml

kubectl -n sitecheck rollout status deployment/sitecheck --timeout=180s
kubectl -n sitecheck get pods -o wide
kubectl -n sitecheck get service
```

Ожидается две готовые реплики: обе имеют статус `Running` и `READY 1/1`.

Для доступа к сервису с локальной машины запустите port-forward и оставьте команду работающей:

```bash
kubectl -n sitecheck port-forward svc/sitecheck 8080:80
```

В другом терминале проверьте маршруты:

```bash
curl -fsS http://127.0.0.1:8080/health
curl -fsS http://127.0.0.1:8080/ready
curl -fsS http://127.0.0.1:8080/version
```

Проверить API с токеном:

```bash
TOKEN="$(cat ~/.sitecheck-api-token)"
curl -fsS -X POST \
  -H "X-Api-Token: $TOKEN" \
  --data-binary 'https://example.com/' \
  http://127.0.0.1:8080/api/check
unset TOKEN
```

Токен хранится в Kubernetes Secret, а не в Git. `APP_PORT` и другие обычные настройки передаются через ConfigMap.

## Эксплуатация Kubernetes

### Балансировка

Выполнить последовательные запросы к Service из временного pod:

```bash
kubectl -n sitecheck run probe --rm -i --restart=Never \
  --image=curlimages/curl -- \
  sh -c 'for i in $(seq 1 20); do curl -s http://sitecheck/version; echo; sleep 0.5; done'
```

Поле `hostname` в ответе показывает, какой pod обслужил запрос. Ответы с именами разных pod подтверждают балансировку между готовыми репликами.

### Самовосстановление

Удалить один pod:

```bash
POD="$(kubectl -n sitecheck get pods -l app=sitecheck \
  -o jsonpath='{.items[0].metadata.name}')"

kubectl -n sitecheck delete pod "$POD"
kubectl -n sitecheck rollout status deployment/sitecheck --timeout=180s
kubectl -n sitecheck get deployment sitecheck
kubectl -n sitecheck get pods -o wide
```

Deployment должен автоматически создать замену. После восстановления снова должны быть готовы две реплики.

### Масштабирование

Временно увеличить количество реплик до трёх и дождаться их готовности:

```bash
kubectl -n sitecheck scale deployment/sitecheck --replicas=3
kubectl -n sitecheck wait --for=condition=Ready pod \
  -l app=sitecheck --timeout=180s
kubectl -n sitecheck get deployment sitecheck
kubectl -n sitecheck get pods -o wide
```

Вернуть число реплик, заданное декларативно в манифесте:

```bash
kubectl apply -f k8s/deployment.yaml
kubectl -n sitecheck rollout status deployment/sitecheck --timeout=180s
kubectl -n sitecheck get deployment sitecheck
```

В `deployment.yaml` задано `replicas: 2`, поэтому после применения манифеста Deployment должен вернуться к двум репликам.

### Наблюдение при обновлении

Для наблюдения за ответами сервиса запустите цикл запросов в отдельном терминале:

```bash
kubectl -n sitecheck run probe --rm -it --restart=Never \
  --image=curlimages/curl -- \
  sh -c 'while true; do curl -fsS http://sitecheck/version; echo; sleep 0.5; done'
```

Остановить цикл — `Ctrl+C`.

### Обновление без простоя

Перед обновлением сначала выпустите новую версию образа через CI. Образ версии `1.0.1` должен быть опубликован в Docker Hub до того, как этот тег будет внесён в манифест.

В `k8s/deployment.yaml` укажите опубликованный образ:

```yaml
image: cerafim125/sitecheck:1.0.1
```

Применить обновление и наблюдать его ход:

```bash
kubectl apply -f k8s/deployment.yaml
kubectl -n sitecheck rollout status deployment/sitecheck --timeout=180s
kubectl -n sitecheck rollout history deployment/sitecheck
```

При успешном обновлении новые pod становятся готовыми до удаления старых. Цикл запросов к `/version` показывает смену версии; ошибок в ответах быть не должно. Стратегия `RollingUpdate` с `maxUnavailable: 0` не позволяет уменьшить количество доступных реплик во время обновления.

### Откат

Вернуться к предыдущей ревизии Deployment:

```bash
kubectl -n sitecheck rollout undo deployment/sitecheck
kubectl -n sitecheck rollout status deployment/sitecheck --timeout=180s
kubectl -n sitecheck rollout history deployment/sitecheck
```

Проверить версию образа, используемую Deployment:

```bash
kubectl -n sitecheck get deployment sitecheck \
  -o jsonpath='{.spec.template.spec.containers[0].image}{"\n"}'
```

После отката поддерживайте `k8s/deployment.yaml` в соответствии с выбранной рабочей версией образа.

### Неудачный выпуск

Для проверки обработки ошибочного выпуска временно укажите несуществующий тег, например `cerafim125/sitecheck:9.9.9`, в `k8s/deployment.yaml`. Примените манифест:

```bash
kubectl apply -f k8s/deployment.yaml
kubectl -n sitecheck get pods -o wide
```

Посмотреть причину сбоя:

```bash
kubectl -n sitecheck describe pods
kubectl -n sitecheck get events --sort-by=.lastTimestamp
```

Для нового pod ожидается `ImagePullBackOff` или `ErrImagePull`. При `maxUnavailable: 0` уже готовые старые pod должны оставаться доступными, пока новые не готовы.

Восстановить предыдущую ревизию:

```bash
kubectl -n sitecheck rollout undo deployment/sitecheck
kubectl -n sitecheck rollout status deployment/sitecheck --timeout=180s
kubectl -n sitecheck rollout history deployment/sitecheck
```

После этого верните в `k8s/deployment.yaml` существующий рабочий тег и примените манифест:

```bash
kubectl apply -f k8s/deployment.yaml
kubectl -n sitecheck rollout status deployment/sitecheck --timeout=180s
```

### Диагностика

Проверить состояние Deployment, pod и события:

```bash
kubectl -n sitecheck get deployment
kubectl -n sitecheck get pods -o wide
kubectl -n sitecheck describe pods
kubectl -n sitecheck get events --sort-by=.lastTimestamp
```

Посмотреть журналы контейнеров:

```bash
kubectl -n sitecheck logs -l app=sitecheck \
  --all-containers=true --prefix=true
```

При `ImagePullBackOff` проверьте имя Docker Hub namespace, репозитория и тега, а также доступность образа. При `CrashLoopBackOff` изучите журналы контейнера и причину завершения в `kubectl describe pod`.

### Проверка потребления ресурсов

В манифесте указаны начальные ресурсы:

- requests: CPU `50m`, память `32Mi`;
- limits: CPU `250m`, память `64Mi`.

## Практические работы

- **ПР 1:** программа на C, CMake, модуль `core` и deb-пакет.
- **ПР 2:** CTest, санитайзеры, анализ покрытия, cppcheck, lintian и GitHub Actions.
- **ПР 3:** HTTP-сервис, Docker-образ из `.deb`, smoke-тест и публикация образа.
- **ПР 4:** Kubernetes-манифесты, балансировка, самовосстановление, масштабирование, обновление, откат и диагностика.

## Структура репозитория

```text
.
├── src/                    # исходный код программы и HTTP-сервера
├── tests/                  # тесты
├── packaging/              # файлы deb-пакета
├── .github/workflows/      # конвейер GitHub Actions
├── k8s/                    # Kubernetes-манифесты
├── build-deps.txt          # список зависимостей сборки
├── build_deb.sh            # сценарий сборки deb-пакета
├── Dockerfile              # сборка образа из deb-пакета
├── VERSION                 # версия программы
└── README.md
```

В Git не добавляются Secret, реальные токены, файл `~/.sitecheck-api-token`, каталоги сборки и собранные `.deb`-пакеты.
