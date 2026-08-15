# Arbuz

[English](README.md)

**Табличный калькулятор** для **Windows 10/11** и **Linux (x86_64, glibc)**.  
Лицензия: [MIT](LICENSE).

Сетка, которая считает: числа и формулы в ячейках, результат сразу на экране. Не Excel и не LibreOffice Calc — без графиков, сводных таблиц, ODS и совместного редактирования.

Первая электронная таблица называлась VisiCalc (*visible calculator*). Arbuz идёт туда же: калькулятор с памятью в виде сетки, а не комбайн для отчётов.

## Возможности

- Несколько листов, строка формул, отмена, вставка строк и столбцов
- 74 функции (`SUM`, `SUMIFS`, `IF`, `VLOOKUP`, `TODAY`, …), ссылки на другие листы
- Десятичная запятая и проценты: `1,5`, `25%`, `=A1*0,13`
- Маркер заполнения, Ctrl+D / Ctrl+R, поиск F3, печать
- Свои `.xlsx` и `.csv` (книга Arbuz открывается обратно со стилями, объединением, закреплением)
- Темы: белая, тёмная, арбузная
- Плагины на системном Python 3 (интерпретатор в архив не входит)

## Скачать / portable

Готовые zip (Windows и Linux): [syfaren.github.io](https://syfaren.github.io/) и [Releases](https://github.com/SYFaren/Arbuz/releases).

Собрать portable из исходников:

```bash
./scripts/package-linux.sh      # → dist/Arbuz-linux-x86_64.zip
./scripts/package-windows.sh    # → dist/Arbuz-windows-x86_64.zip  (нужен MinGW/Qt под Windows)
```

После распаковки:

- **Windows:** `Arbuz.exe` в корне папки (не `runtime\engine.exe`)
- **Linux:** `./Arbuz.sh` в корне папки

Рядом с запускателем: `README.txt`, `README.ru.txt`, `CREDITS.md`, `CREDITS.ru.md`, `languages/`, `themes/`, `plugins/`. Движок и Qt — в `runtime/`, эту папку не трогать.

## Сборка из исходников

Нужны **Qt 6** (Widgets + PrintSupport), **CMake ≥ 3.16**, **zlib**.

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build -j
./build/arbuz
```

Хелпер для Linux: [`scripts/build.sh`](scripts/build.sh).

Проверки:

```bash
./build/arbuz --self-test
QT_QPA_PLATFORM=offscreen ./build/arbuz --ui-test
./scripts/run-tests.sh
```

Язык интерфейса при первом запуске берётся из системы (русский или английский), потом меняется в **Вид → Настройки**. Строки — в `resources/i18n/*.json`, в portable лежат в `languages/`; чтобы добавить язык, положите ещё один `xx.json` без пересборки.

## Плагины

Папка `plugins/` рядом с программой (в portable — рядом с запускателем, не внутри `runtime/`). Нужен Python 3 в `PATH`. Описание: [`plugins/README.txt`](plugins/README.txt) / [`plugins/README.ru.txt`](plugins/README.ru.txt). Меню **Плагины**.

## Структура репозитория

| Путь | Назначение |
|------|------------|
| `src/app/` | Точка входа, главное окно, Windows-лаунчер |
| `src/core/` | Книга, сетка, ссылки, форматы |
| `src/formula/` | Движок формул |
| `src/io/` | xlsx / csv |
| `src/ui/` | Темы, диалоги, i18n |
| `src/plugins/` | Хост Python-плагинов |
| `src/test/` | `--self-test`, `--ui-test` |
| `scripts/` | Сборка, тесты, portable zip |
| `third_party/` | Атрибуция (QXlsx не линкуется; xlfparser — заголовок) |

## Заимствования

Список — [CREDITS.ru.md](CREDITS.ru.md) (русский) и [CREDITS.md](CREDITS.md) (английский), в программе: **Справка → Благодарности**. В JSON языка в `_meta` можно указать `"credits"`, какой текст показывать (или взять английский).
