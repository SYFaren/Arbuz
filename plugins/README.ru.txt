Плагины Arbuz (Python 3)
========================

Эта папка лежит РЯДОМ с Arbuz.exe / Arbuz.sh — её видно сразу, без поиска
среди DLL в runtime/.

Как добавить плагин
-------------------
1. Создайте подпапку, например my-tools/
2. Положите туда plugin.py
3. Меню «Плагины» → «Перезагрузить плагины» (или перезапустите Arbuz)

Нужен Python 3, установленный в системе (python3 / python.exe в PATH).
В portable-архив интерпретатор НЕ кладётся — zip остаётся маленьким,
а ярлык запуска не тонет в тысячах файлов.

Минимальный plugin.py
---------------------
    import arbuz

    @arbuz.function("VAT", syntax="VAT(amount)", help_ru="НДС 20% от суммы")
    def vat(amount):
        return float(amount or 0) * 0.20

    @arbuz.command("fill.hello", "Привет в текущую ячейку", "Hello into current cell")
    def hello():
        arbuz.set(arbuz.current_cell(), "привет")

    @arbuz.on("cell_changed")
    def changed(msg):
        pass  # msg["a1"], msg["sheet"]

API (модуль arbuz)
------------------
Функции и меню:
  @function(name, syntax=..., help_ru=..., help_en=...)
  @command(id, title_ru, title_en="")
  @on(event)          app_start, app_quit, cell_changed, sheet_changed,
                      selection_changed, workbook_new, workbook_opened,
                      before_save, after_save
  error("#VALUE!")    вернуть ошибку из функции плагина

Ячейки:
  get("B2") / get_raw("B2") / set("B2", value)
  get_range("A1:C3") / set_range("A1:C3", [[1,2,3], ...])
  current_cell() / selection()
  Cell("A1"), Range("A1:B10")
  a1(row, col)        1-based, например a1(1, 1) == "A1"
  numbers(values)     вытащить числа из диапазона / вложенных списков

Листы:
  sheets() / sheet_count() / current_sheet() / current_sheet_name()
  set_current_sheet(i) / add_sheet("Имя") / rename_sheet("Имя") / remove_sheet()
  row_count() / col_count()

Формулы плагина появляются в Вставка → Функция, категория «Плагины».
В функции диапазон (VAT(A1:A3) или =MY(A1:B2)) приходит списком значений.

Примеры лежат в исходниках Arbuz (`example-vat`, `example-hello`, `example-tools`), в portable-zip их нет.

Переменные окружения:
  ARBUZ_PYTHON         полный путь к python, если его нет в PATH
  ARBUZ_NO_PLUGINS=1   не запускать хост (для тестов)

English: see README.txt
