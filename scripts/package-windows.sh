#!/usr/bin/env bash
# Cross-builds a Windows portable zip (MinGW + Qt win64_mingw).
#
# Zip root (easy to find the exe):
#   Arbuz.exe          — launcher, no Qt DLLs
#   README.txt
#   CREDITS.md
#   python-plugins/    — user Python plugins
#   runtime/           — engine.exe + Qt + python-host
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QT_WIN="${ARBUZ_QT_WIN:-${HOME}/Qt/6.5.3/mingw_64}"
QT_HOST="${ARBUZ_QT_HOST:-${HOME}/Qt/6.5.3/gcc_64}"
MINGW="${ARBUZ_MINGW:-${HOME}/.local/opt/mingw-gcc}"
OUT="${ROOT}/dist/Arbuz-windows-x86_64"
ZIP="${ROOT}/dist/Arbuz-windows-x86_64.zip"
BUILD="${ROOT}/build-win"
ZBUILD="${ROOT}/build-win-zlib"
RUNTIME="${OUT}/runtime"

find_cc() {
  if [[ -x "${MINGW}/bin/x86_64-w64-mingw32-g++" ]]; then
    echo "${MINGW}/bin/x86_64-w64-mingw32-g++"
    return
  fi
  command -v x86_64-w64-mingw32-g++ || true
}

CC="$(find_cc)"
if [[ -z "${CC}" ]]; then
  echo "No MinGW g++. Set ARBUZ_MINGW to an xPack mingw-w64-gcc prefix." >&2
  exit 2
fi
if [[ ! -d "${QT_WIN}" ]]; then
  echo "Windows Qt not found at ${QT_WIN}." >&2
  echo "Install: aqt install-qt windows desktop 6.5.3 win64_mingw -O \$HOME/Qt" >&2
  exit 2
fi

BIN_DIR="$(dirname "${CC}")"
export PATH="${BIN_DIR}:${PATH}"
CMAKE="${HOME}/.local/opt/cmake/bin/cmake"
[[ -x "${CMAKE}" ]] || CMAKE="$(command -v cmake)"
C_CC="${BIN_DIR}/x86_64-w64-mingw32-gcc"
RC="${BIN_DIR}/x86_64-w64-mingw32-windres"

rm -rf "${BUILD}" "${OUT}" "${ZBUILD}"
mkdir -p "${BUILD}" "${OUT}" "${ZBUILD}" "${ROOT}/.deps/zlib-src"

if [[ ! -f "${ROOT}/.deps/zlib-src/zlib.h" ]]; then
  curl -L --fail -o /tmp/zlib.tar.gz "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz"
  tar -xzf /tmp/zlib.tar.gz -C "${ROOT}/.deps"
  rm -rf "${ROOT}/.deps/zlib-src"
  mv "${ROOT}/.deps/zlib-1.3.1" "${ROOT}/.deps/zlib-src"
fi

"${CMAKE}" -S "${ROOT}/.deps/zlib-src" -B "${ZBUILD}" \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER="${C_CC}" \
  -DCMAKE_RC_COMPILER="${RC}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DZLIB_BUILD_EXAMPLES=OFF
"${CMAKE}" --build "${ZBUILD}" -j"$(nproc)" --target zlibstatic
cp -a "${ZBUILD}/zconf.h" "${ROOT}/.deps/zlib-src/zconf.h"
ZLIB_A="$(find "${ZBUILD}" -name 'libzlibstatic.a' -o -name 'libz.a' | head -1)"

"${CMAKE}" -S "${ROOT}" -B "${BUILD}" \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER="${C_CC}" \
  -DCMAKE_CXX_COMPILER="${CC}" \
  -DCMAKE_RC_COMPILER="${RC}" \
  -DCMAKE_PREFIX_PATH="${QT_WIN}" \
  -DQT_HOST_PATH="${QT_HOST}" \
  -DCMAKE_FIND_ROOT_PATH="${QT_WIN}" \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
  -DCMAKE_BUILD_TYPE=Release \
  -DZLIB_INCLUDE_DIR="${ROOT}/.deps/zlib-src" \
  -DZLIB_LIBRARY="${ZLIB_A}"

"${CMAKE}" --build "${BUILD}" -j"$(nproc)"

mkdir -p "${RUNTIME}/qt-plugins/platforms" \
  "${RUNTIME}/qt-plugins/imageformats" \
  "${RUNTIME}/qt-plugins/styles" \
  "${RUNTIME}/themes" \
  "${RUNTIME}/python-host" \
  "${OUT}/python-plugins"

LAUNCHER="${BUILD}/arbuz-launcher.exe"
ENGINE="${BUILD}/arbuz.exe"
if [[ ! -f "${LAUNCHER}" ]]; then
  echo "missing launcher: ${LAUNCHER}" >&2
  exit 1
fi
cp -a "${LAUNCHER}" "${OUT}/Arbuz.exe"
cp -a "${ENGINE}" "${RUNTIME}/engine.exe"

cp -a "${QT_WIN}/bin/Qt6Core.dll" "${QT_WIN}/bin/Qt6Gui.dll" "${QT_WIN}/bin/Qt6Widgets.dll" \
  "${QT_WIN}/bin/Qt6PrintSupport.dll" "${RUNTIME}/"
cp -a "${QT_WIN}/bin/Qt6Network.dll" "${RUNTIME}/" 2>/dev/null || true
cp -a "${QT_WIN}/bin/"icu*.dll "${RUNTIME}/" 2>/dev/null || true
cp -a "${QT_WIN}/bin/libgcc_s_seh-1.dll" "${RUNTIME}/" 2>/dev/null || true
cp -a "${QT_WIN}/bin/libstdc++-6.dll" "${RUNTIME}/" 2>/dev/null || true
cp -a "${QT_WIN}/bin/libwinpthread-1.dll" "${RUNTIME}/" 2>/dev/null || true
cp -a "${QT_WIN}/bin/opengl32sw.dll" "${RUNTIME}/" 2>/dev/null || true
cp -a "${QT_WIN}/bin/d3dcompiler_47.dll" "${RUNTIME}/" 2>/dev/null || true
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
  if [[ ! -f "${RUNTIME}/${dll}" ]]; then
    find "${MINGW}" -name "${dll}" | head -1 | xargs -r -I{} cp -a {} "${RUNTIME}/"
  fi
done
cp -a "${QT_WIN}/plugins/platforms/qwindows.dll" "${RUNTIME}/qt-plugins/platforms/"
cp -a "${QT_WIN}/plugins/imageformats/"*.dll "${RUNTIME}/qt-plugins/imageformats/" 2>/dev/null || true
cp -a "${QT_WIN}/plugins/styles/"*.dll "${RUNTIME}/qt-plugins/styles/" 2>/dev/null || true
cp -a "${ROOT}/resources/themes/"*.json "${RUNTIME}/themes/"
cp -a "${ROOT}/python/." "${RUNTIME}/python-host/"
cp -a "${ROOT}/python-plugins/." "${OUT}/python-plugins/"
cp -a "${ROOT}/CREDITS.md" "${OUT}/CREDITS.md"

cat > "${RUNTIME}/qt.conf" << 'EOF'
[Paths]
Plugins = qt-plugins
EOF

cat > "${OUT}/README.txt" << 'EOF'
Arbuz — табличный калькулятор, portable Windows (x86_64)
Создатель: SYFaren

════════════════════════════════════
  ЗАПУСК:  Arbuz.exe   (этот файл в корне папки)
════════════════════════════════════

Не открывайте папку runtime — там Qt, DLL и движок.
Не запускайте runtime\engine.exe напрямую.

Плагины Python: папка python-plugins рядом с Arbuz.exe.
Нужен Python 3 в системе (в архив не входит, чтобы zip был маленьким).
Меню «Плагины» → «Открыть папку плагинов…»

Темы: Белая, Тёмная, Арбуз. Свои — Вид → Настройки → Сохранить как…
(файлы тем пишутся в runtime\themes).
EOF

python3 - << PY
import os, zipfile
os.chdir("${ROOT}/dist")
root = "Arbuz-windows-x86_64"
zip_path = "Arbuz-windows-x86_64.zip"
with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
    for dirpath, _, files in os.walk(root):
        for f in files:
            p = os.path.join(dirpath, f)
            z.write(p, p)
print("wrote", os.path.abspath(zip_path), os.path.getsize(zip_path))
PY

echo "Windows portable: ${ZIP}"
echo "Root files:"
ls -1 "${OUT}"
