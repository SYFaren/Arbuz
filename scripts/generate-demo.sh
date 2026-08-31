#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/demo/Arbuz-feature-demo.xlsx"
mkdir -p "${ROOT}/demo"
QT="${HOME}/Qt/6.5.3/gcc_64"
if [[ ! -x "${ROOT}/build/arbuz" ]]; then
  cmake -S "${ROOT}" -B "${ROOT}/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="${QT}"
  cmake --build "${ROOT}/build" -j"$(nproc)"
fi
"${ROOT}/build/arbuz" --write-demo "${OUT}"
echo "Generated: ${OUT}"
echo "Open with: ${ROOT}/build/arbuz --open-demo"
echo "     or: ${ROOT}/build/arbuz \"${OUT}\""
