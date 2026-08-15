#!/usr/bin/env bash
# Linux portable zip.
#
# Zip root (easy to find the launcher):
#   Arbuz.sh           — start here
#   README.txt
#   CREDITS.md
#   python-plugins/    — user Python plugins
#   runtime/           — binary, lib, Qt plugins, python-host
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QT="${HOME}/Qt/6.5.3/gcc_64"
OUT="${ROOT}/dist/Arbuz-linux-x86_64"
ZIP="${ROOT}/dist/Arbuz-linux-x86_64.zip"
RUNTIME="${OUT}/runtime"

"${ROOT}/scripts/build.sh"

rm -rf "${OUT}"
mkdir -p "${RUNTIME}/lib" \
  "${RUNTIME}/qt-plugins/platforms" \
  "${RUNTIME}/qt-plugins/imageformats" \
  "${RUNTIME}/qt-plugins/platformthemes" \
  "${RUNTIME}/qt-plugins/xcbglintegrations" \
  "${RUNTIME}/themes" \
  "${RUNTIME}/python-host" \
  "${RUNTIME}/icons/hicolor/16x16/apps" \
  "${RUNTIME}/icons/hicolor/24x24/apps" \
  "${RUNTIME}/icons/hicolor/32x32/apps" \
  "${RUNTIME}/icons/hicolor/48x48/apps" \
  "${RUNTIME}/icons/hicolor/64x64/apps" \
  "${RUNTIME}/icons/hicolor/128x128/apps" \
  "${RUNTIME}/icons/hicolor/256x256/apps" \
  "${RUNTIME}/icons/hicolor/512x512/apps" \
  "${RUNTIME}/icons/hicolor/scalable/apps" \
  "${OUT}/python-plugins"

cp -a "${ROOT}/build/arbuz" "${RUNTIME}/arbuz"
chmod +x "${RUNTIME}/arbuz"

copy_lib() {
  local src="$1"
  local name
  name="$(basename "$src")"
  [[ -f "${RUNTIME}/lib/${name}" ]] && return 0
  [[ -f "$src" ]] || return 0
  cp -a "$src" "${RUNTIME}/lib/"
  if [[ -L "$src" ]]; then
    local real
    real="$(readlink -f "$src")"
    cp -a "$real" "${RUNTIME}/lib/" 2>/dev/null || true
  fi
}

mapfile -t NEEDED < <(ldd "${RUNTIME}/arbuz" | awk '/=>/ {print $3}' | grep -E '/Qt/|libicu|libpcre2|libz\.so|libglib|libgthread|libgobject|libpng|libharfbuzz|libfreetype|libfontconfig|libmd4c|libdouble-conversion|libb2' || true)
for so in "${NEEDED[@]}"; do
  copy_lib "$so"
done

copy_lib /lib/x86_64-linux-gnu/libz.so.1 || copy_lib /usr/lib/x86_64-linux-gnu/libz.so.1 || true

for extra in \
  "${QT}/lib/libQt6Core.so.6" \
  "${QT}/lib/libQt6Gui.so.6" \
  "${QT}/lib/libQt6Widgets.so.6" \
  "${QT}/lib/libQt6DBus.so.6" \
  "${QT}/lib/libQt6XcbQpa.so.6" \
  "${QT}/lib/libQt6OpenGL.so.6" \
  "${QT}/lib/libicuuc.so.56" \
  "${QT}/lib/libicui18n.so.56" \
  "${QT}/lib/libicudata.so.56"
do
  copy_lib "$extra"
done

if [[ -f "${QT}/lib/libQt6XcbQpa.so.6" ]]; then
  mapfile -t XCBDEPS < <(ldd "${QT}/lib/libQt6XcbQpa.so.6" | awk '/=>/ {print $3}' | grep -E '/Qt/|libicu' || true)
  for so in "${XCBDEPS[@]}"; do
    copy_lib "$so"
  done
fi

cp -a "${QT}/plugins/platforms/libqxcb.so" "${RUNTIME}/qt-plugins/platforms/" 2>/dev/null || true
cp -a "${QT}/plugins/platforms/libqoffscreen.so" "${RUNTIME}/qt-plugins/platforms/" 2>/dev/null || true
cp -a "${QT}/plugins/imageformats/"*.so "${RUNTIME}/qt-plugins/imageformats/" 2>/dev/null || true
cp -a "${QT}/plugins/platformthemes/"*.so "${RUNTIME}/qt-plugins/platformthemes/" 2>/dev/null || true
cp -a "${QT}/plugins/xcbglintegrations/"*.so "${RUNTIME}/qt-plugins/xcbglintegrations/" 2>/dev/null || true

if [[ -f "${RUNTIME}/qt-plugins/platforms/libqxcb.so" ]]; then
  mapfile -t PDEPS < <(ldd "${RUNTIME}/qt-plugins/platforms/libqxcb.so" | awk '/=>/ {print $3}' | grep '/Qt/' || true)
  for so in "${PDEPS[@]}"; do
    copy_lib "$so"
  done
fi

cp -a "${ROOT}/resources/themes/"*.json "${RUNTIME}/themes/"
cp -a "${ROOT}/python/." "${RUNTIME}/python-host/"
cp -a "${ROOT}/python-plugins/." "${OUT}/python-plugins/"
cp -a "${ROOT}/CREDITS.md" "${OUT}/CREDITS.md"
cp -a "${ROOT}/resources/arbuz.desktop" "${RUNTIME}/arbuz.desktop"
cp -a "${ROOT}/resources/icons/arbuz-256.png" "${RUNTIME}/arbuz.png"
cp -a "${ROOT}/resources/icons/arbuz.svg" "${RUNTIME}/icons/hicolor/scalable/apps/arbuz.svg"
for sz in 16 24 32 48 64 128 256 512; do
  cp -a "${ROOT}/resources/icons/arbuz-${sz}.png" "${RUNTIME}/icons/hicolor/${sz}x${sz}/apps/arbuz.png"
done

cat > "${RUNTIME}/qt.conf" << 'EOF'
[Paths]
Plugins = qt-plugins
EOF

cat > "${OUT}/Arbuz.sh" << 'EOF'
#!/bin/sh
ROOT="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
export ARBUZ_PORTABLE_ROOT="${ROOT}"
export LD_LIBRARY_PATH="${ROOT}/runtime/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${ROOT}/runtime/qt-plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${ROOT}/runtime/qt-plugins/platforms"
export QT_QPA_FONTDIR="${QT_QPA_FONTDIR:-/usr/share/fonts}"
exec "${ROOT}/runtime/arbuz" "$@"
EOF
chmod +x "${OUT}/Arbuz.sh"

cat > "${OUT}/README.txt" << 'EOF'
Arbuz — табличный калькулятор, portable Linux (x86_64)
Создатель: SYFaren

════════════════════════════════════
  ЗАПУСК:  ./Arbuz.sh   (этот файл в корне папки)
════════════════════════════════════

Не открывайте папку runtime — там библиотеки Qt.
Нужна обычная Linux-система с glibc. Шрифты — из /usr/share/fonts.

Плагины Python: папка python-plugins рядом с Arbuz.sh.
Нужен python3 в системе (в архив не входит).
Меню «Плагины» → «Открыть папку плагинов…»

Темы: Белая, Тёмная, Арбуз.
Свои темы: Вид → Настройки → Сохранить как… (runtime/themes).

Формулы: Вставка → Функция, или правый щелчок по ячейке.
EOF

(
  cd "${ROOT}/dist"
  rm -f "${ZIP}"
  python3 - << PY
import os, zipfile
root = "Arbuz-linux-x86_64"
zip_path = "Arbuz-linux-x86_64.zip"
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
    for dirpath, _, files in os.walk(root):
        for f in files:
            p = os.path.join(dirpath, f)
            z.write(p, p)
print("wrote", os.path.abspath(zip_path), os.path.getsize(zip_path))
PY
)

echo "Linux portable: ${ZIP}"
echo "Root files:"
ls -1 "${OUT}"
