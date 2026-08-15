#!/usr/bin/env bash
# Run every Arbuz check: build, C++ self/ui tests, Python plugins, icons, portable zips.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="${HOME}/.local/opt/cmake/bin:${PATH}"
export LD_LIBRARY_PATH="${HOME}/Qt/6.5.3/gcc_64/lib:${LD_LIBRARY_PATH:-}"
BIN="${ROOT}/build/arbuz"
fails=0

run() {
  local name="$1"
  shift
  echo
  echo "======== ${name} ========"
  if "$@"; then
    echo "PASS ${name}"
  else
    echo "FAIL ${name}" >&2
    fails=$((fails + 1))
  fi
}

run "build" "${ROOT}/scripts/build.sh"
run "self-test" "${BIN}" --self-test
run "ui-test" env QT_QPA_PLATFORM=offscreen "${BIN}" --ui-test
run "python-helpers" python3 "${ROOT}/python/tests/test_helpers.py"
run "python-plugin-protocol" python3 "${ROOT}/python/tests/test_plugin_protocol.py"
run "python-compile" python3 -m py_compile \
  "${ROOT}/python/arbuz/__init__.py" \
  "${ROOT}/python/plugin_runner.py" \
  "${ROOT}/plugins/example-vat/plugin.py" \
  "${ROOT}/plugins/example-hello/plugin.py" \
  "${ROOT}/plugins/example-tools/plugin.py"

echo
echo "======== icons ========"
for f in arbuz-16.png arbuz-32.png arbuz-64.png arbuz-256.png arbuz.ico arbuz.svg; do
  if [[ -s "${ROOT}/resources/icons/${f}" ]]; then
    echo "PASS icon ${f}"
  else
    echo "FAIL icon missing ${f}" >&2
    fails=$((fails + 1))
  fi
done

echo
echo "======== portable zips ========"
check_zip() {
  local zip="$1"
  local expect_launcher="$2"
  python3 - "$zip" "$expect_launcher" << 'PY'
import sys, zipfile
path, launcher = sys.argv[1], sys.argv[2]
z = zipfile.ZipFile(path)
root = path.rsplit("/", 1)[-1].removesuffix(".zip") + "/"
kids = sorted({p[len(root):].split("/")[0] for p in z.namelist() if p.startswith(root) and p != root})
need = {launcher, "README.txt", "README.ru.txt", "CREDITS.md", "CREDITS.ru.md", "plugins", "runtime", "languages", "themes"}
missing = need - set(kids)
if missing:
    print("FAIL zip missing", missing)
    sys.exit(1)
if "lib" in kids or "bin" in kids:
    print("FAIL zip still has old lib/bin at root", kids)
    sys.exit(1)
dlls = [k for k in kids if k.lower().endswith(".dll")]
if dlls:
    print("FAIL zip has DLLs at root", dlls)
    sys.exit(1)
print("PASS zip root", ", ".join(kids))
PY
}

if [[ -f "${ROOT}/dist/Arbuz-linux-x86_64.zip" ]]; then
  if ! check_zip "${ROOT}/dist/Arbuz-linux-x86_64.zip" "Arbuz.sh"; then
    fails=$((fails + 1))
  fi
else
  echo "SKIP linux zip (not built)"
fi

if [[ -f "${ROOT}/dist/Arbuz-windows-x86_64.zip" ]]; then
  if ! check_zip "${ROOT}/dist/Arbuz-windows-x86_64.zip" "Arbuz.exe"; then
    fails=$((fails + 1))
  fi
else
  echo "SKIP windows zip (not built)"
fi

echo
if [[ "${fails}" -eq 0 ]]; then
  echo "ALL TESTS PASSED"
  exit 0
fi
echo "FAILED SUITES: ${fails}" >&2
exit 1
