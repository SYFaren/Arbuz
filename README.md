# Arbuz

[Русский](README.ru.md)

**Table calculator** for **Windows 10/11** and **Linux (x86_64, glibc)**.  
License: [MIT](LICENSE).

A grid that calculates: numbers and formulas in cells, results on screen. Not Excel and not LibreOffice Calc — no charts, pivots, ODS, or collaboration.

The first spreadsheet was VisiCalc (*visible calculator*). Arbuz follows that idea: a calculator with a grid for memory, not a report engine.

## Features

- Multiple sheets, formula bar, undo, insert rows/columns
- 74 functions (`SUM`, `SUMIFS`, `IF`, `VLOOKUP`, `TODAY`, …), cross-sheet references
- Decimal comma and percents: `1,5`, `25%`, `=A1*0,13`
- Fill handle, Ctrl+D / Ctrl+R, Find (F3), print
- Own `.xlsx` and `.csv` (round-trip keeps styles, merges, freeze panes)
- Themes: white, dark, watermelon
- Plugins on system Python 3 (interpreter is not bundled)

## Download / portable build

Ready-to-run zips (Windows and Linux) are on the [Releases](https://github.com/SYFaren/Arbuz/releases) page.

To build a portable zip from source:

```bash
./scripts/package-linux.sh      # → dist/Arbuz-linux-x86_64.zip
./scripts/package-windows.sh    # → dist/Arbuz-windows-x86_64.zip  (needs MinGW/Qt for Windows)
```

After unpacking:

- **Windows:** run `Arbuz.exe` in the folder root (not `runtime\engine.exe`)
- **Linux:** run `./Arbuz.sh` in the folder root

`README.txt`, `README.ru.txt`, `CREDITS.md`, `CREDITS.ru.md`, `languages/`, `themes/`, and `plugins/` sit next to the launcher. The engine and Qt live under `runtime/` — leave that folder alone.

## Build from source

Requires **Qt 6** (Widgets + PrintSupport), **CMake ≥ 3.16**, and **zlib**.

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build -j
./build/arbuz
```

Linux helper (adjust Qt path if needed): [`scripts/build.sh`](scripts/build.sh).

Tests:

```bash
./build/arbuz --self-test
QT_QPA_PLATFORM=offscreen ./build/arbuz --ui-test
./scripts/run-tests.sh
```

UI language follows the system on first run (Russian or English) and can be changed in **View → Settings**. Strings live in `resources/i18n/*.json` and ship as `languages/` next to the portable launcher; add another `xx.json` there to translate without rebuilding.

## Plugins

Put plugins in `plugins/` next to the app (next to the launcher in portable builds, not inside `runtime/`). Needs Python 3 on `PATH`. See [`plugins/README.txt`](plugins/README.txt) / [`plugins/README.ru.txt`](plugins/README.ru.txt). Menu: **Plugins**.

## Repository layout

| Path | Role |
|------|------|
| `src/app/` | Entry point, main window, Windows launcher |
| `src/core/` | Workbook, grid, refs, number formats |
| `src/formula/` | Formula engine |
| `src/io/` | xlsx / csv |
| `src/ui/` | Themes, dialogs, i18n |
| `src/plugins/` | Python plugin host |
| `src/test/` | `--self-test`, `--ui-test` |
| `scripts/` | Build, tests, portable zip |
| `third_party/` | Attribution (QXlsx is not linked; xlfparser is header-only) |

## Credits

Open-source projects used as references are listed in [CREDITS.md](CREDITS.md) (English) and [CREDITS.ru.md](CREDITS.ru.md) (Russian), and in the app under **Help → Credits**. A language JSON may set `"credits"` in `_meta` to pick which document to show (or reuse English).
