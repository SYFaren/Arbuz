#include "demo.h"

#include "chart.h"
#include "fileio.h"
#include "i18n.h"
#include "numformat.h"
#include "workbook.h"

#include <QColor>
#include <cstdio>

static void styleCell(Workbook *wb, int sh, int r, int c, const CellData &base)
{
    wb->setCellData(sh, r, c, base);
}

static void header(Workbook *wb, int sh, int r, int c, const QString &text)
{
    CellData d;
    d.raw = text;
    d.bold = true;
    d.background = QColor(QStringLiteral("#12401c"));
    d.foreground = QColor(Qt::white);
    d.hAlign = 2;
    wb->setCellData(sh, r, c, d);
}

static void note(Workbook *wb, int sh, int r, int c, const QString &text)
{
    CellData d;
    d.raw = text;
    d.foreground = QColor(QStringLiteral("#334155"));
    d.wrap = true;
    wb->setCellData(sh, r, c, d);
}

static void setNumFmt(Workbook *wb, int sh, int r, int c, int fmt)
{
    CellData d = wb->sheet(sh).cell(r, c);
    d.numFmt = fmt;
    wb->setCellData(sh, r, c, d);
}

int writeDemoWorkbook(const QString &path)
{
    Workbook wb;
    buildDemoWorkbook(&wb);
    QString err;
    if (!FileIo::save(&wb, path, &err)) {
        std::fprintf(stderr, "demo save failed: %s\n", qPrintable(err));
        return 1;
    }
    std::printf("demo written: %s\n", qPrintable(path));
    return 0;
}

void buildDemoWorkbook(Workbook *wb)
{
    if (!wb)
        return;
    I18n::setLang(QStringLiteral("ru"));

    wb->resetToEmpty();
    wb->setUndoEnabled(false);
    wb->sheet(0).name = QStringLiteral("README");
    wb->addSheet(QStringLiteral("Formulas"));
    wb->addSheet(QStringLiteral("Formats"));
    wb->addSheet(QStringLiteral("Data"));
    wb->addSheet(QStringLiteral("Charts"));
    wb->addSheet(QStringLiteral("Cross"));
    wb->addSheet(QStringLiteral("Layout"));

    Worksheet &readme = wb->sheet(0);
    readme.columnWidths.insert(0, 420);
    readme.columnWidths.insert(1, 280);
    header(wb, 0, 0, 0, QStringLiteral("Что проверить"));
    header(wb, 0, 0, 1, QStringLiteral("Где / как"));
    const QStringList steps = {
        QStringLiteral("Открытие и сохранение xlsx"),
        QStringLiteral("Файл → Сохранить как… → снова открыть этот файл"),
        QStringLiteral("Несколько листов"),
        QStringLiteral("Вкладки внизу: Formulas, Formats, Data…"),
        QStringLiteral("Строка формул и F4"),
        QStringLiteral("Лист Formulas → клик по формуле → F4 ($A$1)"),
        QStringLiteral("Десятичная запятая и %"),
        QStringLiteral("Formulas: 1,5 и 25%"),
        QStringLiteral("Кросс-листовые формулы"),
        QStringLiteral("Лист Cross и Formulas!B2"),
        QStringLiteral("Копировать / вставить со стилями"),
        QStringLiteral("Formats → выделить A2:D4 → Ctrl+C / Ctrl+V"),
        QStringLiteral("Fill handle, Ctrl+D / Ctrl+R"),
        QStringLiteral("Formulas: потянуть угол A8, Ctrl+D в Data"),
        QStringLiteral("Undo / Redo"),
        QStringLiteral("Ctrl+Z / Ctrl+Y после правки"),
        QStringLiteral("Find / Replace по книге"),
        QStringLiteral("Ctrl+F: FINDME → F3; Ctrl+H: REPLACE_ME → OK"),
        QStringLiteral("AutoFilter"),
        QStringLiteral("Data → выделить A1:D7 → Вид → Автофильтр → Фильтр столбца"),
        QStringLiteral("Сортировка"),
        QStringLiteral("Data → выделить таблицу → Вид → Sort A→Z"),
        QStringLiteral("Merge / Freeze"),
        QStringLiteral("Layout: merge B2:C2; Вид → Закрепить"),
        QStringLiteral("Числовые форматы и валюта"),
        QStringLiteral("Formats: меню Формат → числовой формат, ₽/$/€"),
        QStringLiteral("Диаграммы"),
        QStringLiteral("Charts → готовая диаграмма (--open-demo) или Вставка → Диаграмма"),
        QStringLiteral("Печать"),
        QStringLiteral("Ctrl+P на листе Layout"),
        QStringLiteral("CSV"),
        QStringLiteral("Сохранить лист Formulas как CSV → формулы =… сохраняются"),
        QStringLiteral("Плагины (если есть Python)"),
        QStringLiteral("Плагины → перезагрузить; =VAT(100) в Formulas"),
    };
    int row = 1;
    for (int i = 0; i + 1 < steps.size(); i += 2) {
        note(wb, 0, row, 0, steps.at(i));
        note(wb, 0, row, 1, steps.at(i + 1));
        ++row;
    }

    // --- Formulas ---
    {
        const int sh = 1;
        Worksheet &ws = wb->sheet(sh);
        ws.columnWidths.insert(0, 90);
        ws.columnWidths.insert(1, 90);
        ws.columnWidths.insert(2, 120);
        ws.columnWidths.insert(3, 140);
        header(wb, sh, 0, 0, QStringLiteral("A"));
        header(wb, sh, 0, 1, QStringLiteral("B"));
        header(wb, sh, 0, 2, QStringLiteral("C"));
        header(wb, sh, 0, 3, QStringLiteral("D"));
        wb->setRaw(sh, 1, 0, QStringLiteral("1,5"));
        wb->setRaw(sh, 2, 0, QStringLiteral("2,5"));
        wb->setRaw(sh, 3, 0, QStringLiteral("=SUM(A2:A3)"));
        wb->setRaw(sh, 1, 1, QStringLiteral("25%"));
        wb->setRaw(sh, 2, 1, QStringLiteral("=A2*2"));
        wb->setRaw(sh, 1, 2, QStringLiteral("=Formulas!A2*10"));
        wb->setRaw(sh, 2, 2, QStringLiteral("=IF(A2>1;\"да\";\"нет\")"));
        wb->setRaw(sh, 3, 2, QStringLiteral("=VLOOKUP(\"яблоко\";E5:F7;2;0)"));
        wb->setRaw(sh, 1, 3, QStringLiteral("=TODAY()"));
        wb->setRaw(sh, 2, 3, QStringLiteral("=AVERAGE(A2:A3)"));
        wb->setRaw(sh, 7, 0, QStringLiteral("10"));
        wb->setRaw(sh, 8, 0, QStringLiteral("20"));
        wb->setRaw(sh, 9, 0, QStringLiteral("30"));
        header(wb, sh, 4, 0, QStringLiteral("товар"));
        header(wb, sh, 4, 1, QStringLiteral("цена"));
        wb->setRaw(sh, 5, 0, QStringLiteral("яблоко"));
        wb->setRaw(sh, 5, 1, QStringLiteral("50"));
        wb->setRaw(sh, 6, 0, QStringLiteral("груша"));
        wb->setRaw(sh, 6, 1, QStringLiteral("40"));
        note(wb, sh, 10, 0, QStringLiteral("FINDME — для поиска Ctrl+F"));
    }

    // --- Formats ---
    {
        const int sh = 2;
        header(wb, sh, 0, 0, QStringLiteral("Стиль"));
        header(wb, sh, 0, 1, QStringLiteral("Пример"));
        CellData bold;
        bold.raw = QStringLiteral("Жирный");
        bold.bold = true;
        styleCell(wb, sh, 1, 1, bold);
        CellData italic;
        italic.raw = QStringLiteral("Курсив");
        italic.italic = true;
        styleCell(wb, sh, 2, 1, italic);
        CellData fg;
        fg.raw = QStringLiteral("Красный текст");
        fg.foreground = QColor(QStringLiteral("#b4222e"));
        styleCell(wb, sh, 3, 1, fg);
        CellData bg;
        bg.raw = QStringLiteral("Заливка");
        bg.background = QColor(QStringLiteral("#d4edda"));
        styleCell(wb, sh, 4, 1, bg);
        CellData border;
        border.raw = QStringLiteral("Границы");
        border.border = 15;
        styleCell(wb, sh, 5, 1, border);
        CellData wrap;
        wrap.raw = QStringLiteral("Длинный текст с переносом на несколько строк в одной ячейке");
        wrap.wrap = true;
        styleCell(wb, sh, 6, 1, wrap);
        wb->setRaw(sh, 8, 1, QStringLiteral("1234.567"));
        setNumFmt(wb, sh, 8, 1, NumFormat::Number2);
        wb->setRaw(sh, 9, 1, QStringLiteral("0.25"));
        setNumFmt(wb, sh, 9, 1, NumFormat::Percent);
        wb->setRaw(sh, 10, 1, QStringLiteral("1999.9"));
        setNumFmt(wb, sh, 10, 1, NumFormat::CurrencyRub);
        wb->setRaw(sh, 11, 1, QStringLiteral("42.5"));
        setNumFmt(wb, sh, 11, 1, NumFormat::CurrencyUsd);
        wb->setRaw(sh, 12, 1, QStringLiteral("99.99"));
        setNumFmt(wb, sh, 12, 1, NumFormat::CurrencyEur);
        CellData copyBlock;
        copyBlock.raw = QStringLiteral("Копируй меня");
        copyBlock.bold = true;
        copyBlock.background = QColor(QStringLiteral("#fff3cd"));
        copyBlock.numFmt = NumFormat::Thousands2;
        for (int r = 2; r <= 4; ++r)
            for (int c = 0; c <= 3; ++c) {
                CellData d = copyBlock;
                d.raw = QStringLiteral("R%1C%2").arg(r).arg(c);
                styleCell(wb, sh, r, c, d);
            }
        note(wb, sh, 1, 0, QStringLiteral("Жирный / курсив / цвета"));
        note(wb, sh, 5, 0, QStringLiteral("Границы / перенос"));
        note(wb, sh, 8, 0, QStringLiteral("Числа / % / ₽ / $ / €"));
        note(wb, sh, 2, 0, QStringLiteral("Блок для Ctrl+C → Ctrl+V"));
    }

    // --- Data ---
    {
        const int sh = 3;
        header(wb, sh, 0, 0, QStringLiteral("Отдел"));
        header(wb, sh, 0, 1, QStringLiteral("Товар"));
        header(wb, sh, 0, 2, QStringLiteral("Кол-во"));
        header(wb, sh, 0, 3, QStringLiteral("Цена"));
        const QStringList rows = {
            QStringLiteral("Склад"), QStringLiteral("Яблоки"), QStringLiteral("12"), QStringLiteral("50"),
            QStringLiteral("Склад"), QStringLiteral("Груши"), QStringLiteral("8"), QStringLiteral("40"),
            QStringLiteral("Магазин"), QStringLiteral("Яблоки"), QStringLiteral("5"), QStringLiteral("55"),
            QStringLiteral("Магазин"), QStringLiteral("Сливы"), QStringLiteral("3"), QStringLiteral("70"),
            QStringLiteral("Склад"), QStringLiteral("REPLACE_ME"), QStringLiteral("1"), QStringLiteral("10"),
        };
        int r = 1;
        for (int i = 0; i < rows.size(); i += 4) {
            for (int c = 0; c < 4; ++c)
                wb->setRaw(sh, r, c, rows.at(i + c));
            ++r;
        }
        wb->setRaw(sh, 8, 0, QStringLiteral("=SUM(C2:C7)"));
        note(wb, sh, 9, 0, QStringLiteral("Сортировка и AutoFilter здесь"));
    }

    // --- Charts data ---
    {
        const int sh = 4;
        header(wb, sh, 0, 0, QStringLiteral("Месяц"));
        header(wb, sh, 0, 1, QStringLiteral("Продажи"));
        const char *months[] = {"Янв", "Фев", "Мар", "Апр", "Май"};
        const int sales[] = {120, 150, 90, 180, 140};
        for (int i = 0; i < 5; ++i) {
            wb->setRaw(sh, i + 1, 0, QString::fromUtf8(months[i]));
            wb->setRaw(sh, i + 1, 1, QString::number(sales[i]));
        }
        note(wb, sh, 7, 0, QStringLiteral("Выдели A1:B6 → Вставка → Диаграмма (или --open-demo)"));
        ChartObject chart;
        chart.type = ChartObject::Column;
        chart.srcR1 = 0;
        chart.srcC1 = 0;
        chart.srcR2 = 5;
        chart.srcC2 = 1;
        chart.posX = 280;
        chart.posY = 40;
        chart.widthPx = 380;
        chart.heightPx = 240;
        chart.title = QStringLiteral("Продажи (демо)");
        wb->sheet(sh).charts.append(chart);
    }

    // --- Cross sheet ---
    {
        const int sh = 5;
        wb->setRaw(sh, 0, 0, QStringLiteral("Значение"));
        wb->setRaw(sh, 0, 1, QStringLiteral("42"));
        wb->setRaw(sh, 1, 0, QStringLiteral("=Formulas!A2"));
        wb->setRaw(sh, 2, 0, QStringLiteral("=Cross!B1*2"));
        note(wb, sh, 4, 0, QStringLiteral("Ссылки между листами"));
    }

    // --- Layout ---
    {
        const int sh = 6;
        wb->sheet(sh).freezeRows = 2;
        wb->sheet(sh).freezeCols = 1;
        wb->sheet(sh).rowHeights.insert(1, 32);
        wb->sheet(sh).columnWidths.insert(0, 64);
        wb->sheet(sh).columnWidths.insert(1, 120);
        header(wb, sh, 0, 0, QStringLiteral("№"));
        header(wb, sh, 0, 1, QStringLiteral("Заголовок"));
        header(wb, sh, 0, 2, QStringLiteral("Значение"));
        for (int r = 1; r <= 12; ++r) {
            wb->setRaw(sh, r, 0, QString::number(r));
            wb->setRaw(sh, r, 1, QStringLiteral("Строка %1").arg(r));
            wb->setRaw(sh, r, 2, QString::number(r * 10));
        }
        CellData merged;
        merged.raw = QStringLiteral("Объединено B2:C2");
        merged.hAlign = 2;
        merged.background = QColor(QStringLiteral("#e8f4fc"));
        wb->setCellData(sh, 1, 1, merged);
        wb->mergeCells(sh, 1, 1, 1, 2);
        note(wb, sh, 14, 0, QStringLiteral("Freeze: 1 строка + 1 столбец; merge B2:C2"));
    }

    wb->setUndoEnabled(true);
}
