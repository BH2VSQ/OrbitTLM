#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build=$(realpath "${1:-$repo/build}")
out=$(realpath -m "${2:-$repo/dist}")
tools="$build/appimage-tools"
appdir="$build/OrbitTLM.AppDir"
mkdir -p "$tools" "$out" "$appdir"
if [[ -e "$appdir/AppRun" ]]; then
    echo "AppDir already exists; use a fresh build directory" >&2
    exit 1
fi
fetch() {
    local file=$1 url=$2 checksum=$3
    curl --fail --location --retry 3 --output "$tools/$file" "$url"
    printf '%s  %s\n' "$checksum" "$tools/$file" | sha256sum --check
    chmod +x "$tools/$file"
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724
fetch runtime-x86_64 \
    https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
    2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
DESTDIR="$appdir" cmake --install "$build" --prefix /usr
install -Dm644 "$repo/README.md" "$appdir/usr/share/doc/OrbitTLM/README.md"
export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=x86_64
export VERSION="$(sed -n 's/^project(OrbitTLM VERSION \([^ ]*\).*/\1/p' "$repo/CMakeLists.txt")"
export OUTPUT="$out/OrbitTLM-Linux-x86_64.AppImage"
export LDAI_OUTPUT="$OUTPUT"
export LDAI_VERSION="$VERSION"
export LDAI_RUNTIME_FILE="$tools/runtime-x86_64"
export LDAI_NO_APPSTREAM=1
export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
qt_libs=$("$QMAKE" -query QT_INSTALL_LIBS)
export LD_LIBRARY_PATH="$qt_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# OrbitTLM uses SQLite only. Present a curated plugin tree to linuxdeploy
# without modifying the SDK or pulling in unrelated database clients.
export ORBITTLM_REAL_QMAKE="$QMAKE"
export ORBITTLM_QT_PLUGINS="$tools/qt-plugins"
qt_plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)
mkdir -p "$ORBITTLM_QT_PLUGINS/sqldrivers"
for entry in "$qt_plugins"/*; do
    [[ $(basename "$entry") == sqldrivers ]] && continue
    ln -s "$entry" "$ORBITTLM_QT_PLUGINS/$(basename "$entry")"
done
install -m755 "$qt_plugins/sqldrivers/libqsqlite.so" "$ORBITTLM_QT_PLUGINS/sqldrivers/"
cat > "$tools/qmake-filtered" <<'QMAKE_WRAPPER'
#!/usr/bin/env bash
set -euo pipefail
if [[ ${1-} == -query && ${2-} == QT_INSTALL_PLUGINS ]]; then
    printf '%s\n' "$ORBITTLM_QT_PLUGINS"
else
    "$ORBITTLM_REAL_QMAKE" "$@" | sed "s|^QT_INSTALL_PLUGINS:.*|QT_INSTALL_PLUGINS:$ORBITTLM_QT_PLUGINS|"
fi
QMAKE_WRAPPER
chmod +x "$tools/qmake-filtered"
export QMAKE="$tools/qmake-filtered"
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so;libqminimal.so"
chmod +x "$repo/packaging/linux/AppRun"
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" \
    --executable "$appdir/usr/bin/OrbitTLM" \
    --executable "$appdir/usr/bin/OrbitTLM-cli" \
    --desktop-file "$appdir/usr/share/applications/OrbitTLM.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/orbittlm.svg" \
    --custom-apprun "$repo/packaging/linux/AppRun" --plugin qt --output appimage
for name in Core Gui Widgets Charts Network Sql; do
    test -e "$appdir/usr/lib/libQt6${name}.so.6"
done
test -f "$appdir/usr/plugins/platforms/libqxcb.so"
test -f "$appdir/usr/plugins/sqldrivers/libqsqlite.so"
test -f "$OUTPUT"
chmod +x "$OUTPUT"
(cd "$out" && sha256sum "$(basename "$OUTPUT")" > "$(basename "$OUTPUT").sha256")
