#!/bin/sh
set -eu
umask 022

NAME=sitecheck
VERSION="$(cat VERSION)"

if [ "${GITHUB_REF_TYPE:-}" = "tag" ] &&
   [ "${GITHUB_REF_NAME:-}" != "v$VERSION" ]; then
    echo "Тег ${GITHUB_REF_NAME:-} не совпадает с версией v$VERSION" >&2
    exit 1
fi
ARCH="$(dpkg --print-architecture)"
ROOT="build/pkg/${NAME}_${VERSION}_${ARCH}"

MAINTAINER_NAME="${DEBFULLNAME:-$(git config user.name || true)}"
MAINTAINER_EMAIL="${DEBEMAIL:-$(git config user.email || true)}"

if [ -z "$MAINTAINER_NAME" ] || [ -z "$MAINTAINER_EMAIL" ]; then
    echo "Сначала настройте git config user.name и user.email" >&2
    exit 1
fi

escape_sed() {
    printf '%s' "$1" | sed 's/[\/&]/\\&/g'
}

MAINTAINER="$(escape_sed "$MAINTAINER_NAME <$MAINTAINER_EMAIL>")"

export CFLAGS="$(dpkg-buildflags --get CFLAGS)"
export CPPFLAGS="$(dpkg-buildflags --get CPPFLAGS)"
export LDFLAGS="$(dpkg-buildflags --get LDFLAGS)"

cmake -S . -B build/release \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_C_FLAGS="$CPPFLAGS $CFLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS" \
    -DBUILD_TESTING=OFF

cmake --build build/release --parallel

rm -rf "$ROOT"
mkdir -p "$ROOT"

DESTDIR="$PWD/$ROOT" cmake --install build/release --strip
gzip -9n "$ROOT/usr/share/man/man1/sitecheck.1"

if [ -e debian/control ]; then
    echo "debian/control уже существует; остановка, чтобы не перезаписать его" >&2
    exit 1
fi

mkdir -p debian
trap 'rm -f debian/control; rmdir debian 2>/dev/null || true' EXIT HUP INT TERM
: > debian/control

DEPENDS="$(dpkg-shlibdeps -O -e"$ROOT/usr/bin/$NAME" -e"$ROOT/usr/bin/sitecheck-server" |
    sed -n 's/^shlibs:Depends=//p')"

if [ -z "$DEPENDS" ]; then
    echo "dpkg-shlibdeps не вернул Depends; пакет не создан" >&2
    exit 1
fi

mkdir -p "$ROOT/DEBIAN" "$ROOT/usr/share/doc/$NAME" dist

sed \
    -e "s/@VERSION@/$VERSION/" \
    -e "s/@ARCH@/$ARCH/" \
    -e "s/@DEPENDS@/$DEPENDS/" \
    -e "s/@MAINTAINER@/$MAINTAINER/" \
    packaging/DEBIAN/control > "$ROOT/DEBIAN/control"

install -m 0644 packaging/DEBIAN/conffiles "$ROOT/DEBIAN/conffiles"
install -m 0644 packaging/copyright "$ROOT/usr/share/doc/$NAME/copyright"
install -m 0644 LICENSE "$ROOT/usr/share/doc/$NAME/LICENSE"
gzip -9n -c packaging/changelog \
    > "$ROOT/usr/share/doc/$NAME/changelog.gz"

find "$ROOT" -type d -exec chmod 0755 {} +
find "$ROOT" -type f -exec chmod 0644 {} +
chmod 0755 "$ROOT/usr/bin/$NAME"

dpkg-deb --root-owner-group --build \
    "$ROOT" "dist/${NAME}_${VERSION}_${ARCH}.deb"

echo "Создан пакет dist/${NAME}_${VERSION}_${ARCH}.deb"
