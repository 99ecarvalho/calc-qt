#!/bin/sh
# Build the Debian package from this checkout and put it in dist/.
# Usage: src/linux/packaging/build-deb.sh
# Build dependencies: sudo apt build-dep ./   (run in the repository root)
set -eu

root=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$root"

dpkg-checkbuilddeps || {
    echo "Missing build dependencies. Install them with: sudo apt build-dep ./" >&2
    exit 1
}

version=$(dpkg-parsechangelog -S Version)
arch=$(dpkg-architecture -qDEB_HOST_ARCH)

dpkg-buildpackage --build=binary --no-sign
debian/rules clean >/dev/null

mkdir -p dist
for f in ../calc-qt*_"${version}"_"${arch}".deb \
         ../calc-qt_"${version}"_"${arch}".buildinfo \
         ../calc-qt_"${version}"_"${arch}".changes; do
    [ -e "$f" ] && mv "$f" dist/
done

echo
echo "Packages in $root/dist:"
ls -1 dist/*"${version}"*.deb
echo
echo "Install with: sudo apt install ./dist/calc-qt_${version}_${arch}.deb"
