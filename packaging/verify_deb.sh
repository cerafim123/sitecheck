#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
DEB=${1:-}

if [ -z "$DEB" ] || [ ! -f "$DEB" ]; then
    echo "Использование: $0 путь/к/sitecheck_VERSION_ARCH.deb" >&2
    exit 2
fi

expected_version=$(cat "$REPO_ROOT/VERSION")
package=$(dpkg-deb --field "$DEB" Package)
version=$(dpkg-deb --field "$DEB" Version)
architecture=$(dpkg-deb --field "$DEB" Architecture)
depends=$(dpkg-deb --field "$DEB" Depends)

[ "$package" = sitecheck ] || { echo "Неверное имя пакета: $package" >&2; exit 1; }
[ "$version" = "$expected_version" ] || { echo "Версия пакета не совпадает с VERSION" >&2; exit 1; }
[ -n "$architecture" ] || { echo "Не задана архитектура пакета" >&2; exit 1; }
[ -n "$depends" ] || { echo "Не заданы зависимости пакета" >&2; exit 1; }

contents=$(dpkg-deb --contents "$DEB")
for path in \
    './usr/bin/sitecheck' \
    './etc/sitecheck/sitecheck.conf' \
    './usr/share/doc/sitecheck/copyright' \
    './usr/share/doc/sitecheck/changelog.gz' \
    './usr/share/man/man1/sitecheck.1.gz'
do
    printf '%s\n' "$contents" | grep -Fq "$path" || {
        echo "В пакете отсутствует: $path" >&2
        exit 1
    }
done

echo "Пакет проверен: $package $version ($architecture)"
echo "Depends: $depends"
