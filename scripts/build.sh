#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="${HOME}/.local/opt/cmake/bin:${PATH}"
QT="${ARBUZ_QT:-${HOME}/Qt/6.5.3/gcc_64}"
DEPS="${ROOT}/.deps/usr"
ARGS=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="${QT}")
# Optional local copies of xkbcommon/OpenGL dev files for systems without them.
if [[ -d "${DEPS}" ]]; then
  ARGS=(-DCMAKE_BUILD_TYPE=Release
    -DCMAKE_PREFIX_PATH="${QT};${DEPS}"
    -DXKB_INCLUDE_DIR="${DEPS}/include"
    -DXKB_LIBRARY="${DEPS}/lib/x86_64-linux-gnu/libxkbcommon.so"
    -DOPENGL_INCLUDE_DIR="${DEPS}/include"
    -DOPENGL_opengl_LIBRARY="${DEPS}/lib/x86_64-linux-gnu/libOpenGL.so"
    -DOPENGL_glx_LIBRARY="${DEPS}/lib/x86_64-linux-gnu/libGLX.so"
    -DOPENGL_gl_LIBRARY="${DEPS}/lib/x86_64-linux-gnu/libGL.so")
fi
cmake -S "${ROOT}" -B "${ROOT}/build" "${ARGS[@]}"
cmake --build "${ROOT}/build" -j"$(nproc)"
