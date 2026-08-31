#include "selftest.h"

#include "arbuzicon.h"
#include "cellref.h"
#include "clipdata.h"
#include "fileio.h"
#include "formulaengine.h"
#include "i18n.h"
#include "numformat.h"
#include "pluginhost.h"
#include "sheetmodel.h"
#include "theme.h"
#include "workbook.h"

#include <QAction>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QMimeData>
#include <QPixmap>
#include <QSet>
#include <QTemporaryDir>
#include <QUndoStack>
#include <cstdio>
#include <memory>

static int g_fails = 0;
static int g_ok = 0;

static void fail(const QString &msg)
{
    std::fprintf(stderr, "FAIL %s\n", qPrintable(msg));
    ++g_fails;
}

static void ok(bool cond, const QString &msg)
{
    if (!cond)
        fail(msg);
    else {
        ++g_ok;
        std::printf("OK %s\n", qPrintable(msg));
    }
}

static void eq(const QString &got, const QString &expect, const char *name)
{
    if (got != expect)
        fail(QStringLiteral("%1: got '%2' expected '%3'").arg(QLatin1String(name), got, expect));
    else {
        ++g_ok;
        std::printf("OK %s\n", name);
    }
}

static void testCellRef()
{
    eq(CellRef::columnName(0), QStringLiteral("A"), "col A");
    eq(CellRef::columnName(25), QStringLiteral("Z"), "col Z");
    eq(CellRef::columnName(26), QStringLiteral("AA"), "col AA");
    eq(CellRef::a1(0, 0), QStringLiteral("A1"), "a1 A1");
    eq(CellRef::a1(4, 1), QStringLiteral("B5"), "a1 B5");
    int r = -1, c = -1;
    ok(CellRef::parseA1(QStringLiteral("C12"), &r, &c) && r == 11 && c == 2, QStringLiteral("parse C12"));
    ok(CellRef::parseA1(QStringLiteral("$AA$3"), &r, &c) && r == 2 && c == 26, QStringLiteral("parse $AA$3"));
    ok(!CellRef::parseA1(QStringLiteral("12C"), &r, &c), QStringLiteral("reject 12C"));
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    ok(CellRef::parseA1Range(QStringLiteral("C3:A1"), &r1, &c1, &r2, &c2) && r1 == 0 && c1 == 0 && r2 == 2
           && c2 == 2,
       QStringLiteral("range C3:A1 normalized"));
    ok(CellRef::parseA1Range(QStringLiteral("B2"), &r1, &c1, &r2, &c2) && r1 == 1 && c1 == 1 && r2 == 1 && c2 == 1,
       QStringLiteral("range single B2"));
}

static void testWorkbookSheets()
{
    Workbook wb;
    ok(wb.sheetCount() == 1, QStringLiteral("one sheet by default"));
    const int i = wb.addSheet(QStringLiteral("Данные"));
    ok(i == 1 && wb.sheetCount() == 2 && wb.sheet(1).name == QStringLiteral("Данные"),
       QStringLiteral("add named sheet"));
    ok(wb.renameSheet(1, QStringLiteral("Отчёт")), QStringLiteral("rename sheet"));
    eq(wb.sheet(1).name, QStringLiteral("Отчёт"), "renamed name");
    ok(!wb.removeSheet(99), QStringLiteral("reject bad sheet index"));
    ok(wb.removeSheet(1) && wb.sheetCount() == 1, QStringLiteral("remove extra sheet"));
    ok(!wb.removeSheet(0), QStringLiteral("cannot remove last sheet"));
    wb.setRaw(0, 0, 0, QStringLiteral("привет"));
    eq(wb.displayText(0, 0, 0), QStringLiteral("привет"), "unicode cell");
    wb.setStyle(0, 0, 0, true, true, QColor("#112233"), QColor("#abcdef"));
    ok(wb.sheet(0).cell(0, 0).bold && wb.sheet(0).cell(0, 0).italic, QStringLiteral("bold italic style"));
}

static void testFormulas()
{
    Workbook wb;
    auto put = [&](int row, const QString &raw) { wb.setRaw(0, row, 0, raw); };
    auto shown = [&](int row) { return wb.displayText(0, row, 0); };

    put(0, QStringLiteral("10"));
    put(1, QStringLiteral("20"));
    put(2, QStringLiteral("30"));
    eq(shown(0), QStringLiteral("10"), "literal 10");

    struct Case {
        int row;
        const char *raw;
        const char *expect;
        const char *name;
    };
    const Case cases[] = {
        {3, "=A1+A2", "30", "add"},
        {4, "=A2-A1", "10", "sub"},
        {5, "=A1*A2", "200", "mul"},
        {6, "=A2/A1", "2", "div"},
        {7, "=2+3*4", "14", "precedence"},
        {8, "=(2+3)*4", "20", "parens"},
        {9, "=2^3", "8", "power op"},
        {10, "=SUM(A1:A3)", "60", "SUM"},
        {11, "=PRODUCT(A1:A2)", "200", "PRODUCT"},
        {12, "=ABS(-5)", "5", "ABS"},
        {13, "=SIGN(-3)", "-1", "SIGN"},
        {14, "=SQRT(9)", "3", "SQRT"},
        {15, "=POWER(2,10)", "1024", "POWER"},
        {16, "=MOD(10,3)", "1", "MOD"},
        {17, "=INT(-1.2)", "-2", "INT toward -inf"},
        {18, "=ROUND(2.6,0)", "3", "ROUND"},
        {19, "=ROUNDDOWN(2.9,0)", "2", "ROUNDDOWN"},
        {20, "=ROUNDUP(2.1,0)", "3", "ROUNDUP"},
        {21, "=AVERAGE(A1:A3)", "20", "AVERAGE"},
        {22, "=MIN(A1:A3)", "10", "MIN"},
        {23, "=MAX(A1:A3)", "30", "MAX"},
        {24, "=COUNT(A1:A3)", "3", "COUNT"},
        {25, "=COUNTA(A1:A3)", "3", "COUNTA"},
        {26, "=MEDIAN(A1:A3)", "20", "MEDIAN"},
        {27, "=IF(A1>5,\"yes\",\"no\")", "yes", "IF"},
        {28, "=AND(TRUE,TRUE)", "TRUE", "AND"},
        {29, "=OR(FALSE,TRUE)", "TRUE", "OR"},
        {30, "=NOT(FALSE)", "TRUE", "NOT"},
        {31, "=IFERROR(1/0,0)", "0", "IFERROR"},
        {32, "=TRUE()", "TRUE", "TRUE"},
        {33, "=FALSE()", "FALSE", "FALSE"},
        {34, "=CONCAT(\"a\",\"b\")", "ab", "CONCAT"},
        {35, "=CONCATENATE(\"x\",\"y\")", "xy", "CONCATENATE"},
        {36, "=A1&A2", "1020", "ampersand"},
        {37, "=LEFT(\"hello\",2)", "he", "LEFT"},
        {38, "=RIGHT(\"hello\",2)", "lo", "RIGHT"},
        {39, "=MID(\"hello\",2,3)", "ell", "MID"},
        {40, "=LEN(\"ab\")", "2", "LEN"},
        {41, "=TRIM(\" a  b \")", "a b", "TRIM"},
        {42, "=UPPER(\"Ab\")", "AB", "UPPER"},
        {43, "=LOWER(\"Ab\")", "ab", "LOWER"},
        {44, "=VALUE(\"12\")", "12", "VALUE"},
        {45, "=REPT(\"a\",3)", "aaa", "REPT"},
        {46, "=FIND(\"b\",\"abc\")", "2", "FIND"},
        {47, "=ISBLANK(Z99)", "TRUE", "ISBLANK"},
        {48, "=ISNUMBER(A1)", "TRUE", "ISNUMBER"},
        {49, "=ISTEXT(\"hi\")", "TRUE", "ISTEXT"},
        {50, "=ISERROR(1/0)", "TRUE", "ISERROR"},
        {51, "=N(A1)", "10", "N"},
        {52, "=NA()", "#N/A", "NA"},
        {53, "=1/0", "#DIV/0!", "DIV0"},
        {54, "=NOSUCHFN()", "#NAME?", "unknown fn"},
        {55, "=CHOOSE(2,\"a\",\"b\",\"c\")", "b", "CHOOSE"},
        {56, "=DATE(2020,1,15)", "2020-01-15", "DATE"},
        {57, "=YEAR(DATE(2020,1,15))", "2020", "YEAR"},
        {58, "=MONTH(DATE(2020,1,15))", "1", "MONTH"},
        {59, "=DAY(DATE(2020,1,15))", "15", "DAY"},
        {60, "=COUNTIF(A1:A3,\">15\")", "2", "COUNTIF"},
        {61, "=SUMIF(A1:A3,\">15\")", "50", "SUMIF"},
        {62, "=AVERAGEIF(A1:A3,\">15\")", "25", "AVERAGEIF"},
    };
    for (const Case &c : cases) {
        put(c.row, QString::fromUtf8(c.raw));
        eq(shown(c.row), QString::fromUtf8(c.expect), c.name);
    }

    wb.setRaw(0, 70, 0, QStringLiteral("=PI()"));
    bool pok = false;
    const double piv = wb.displayText(0, 70, 0).toDouble(&pok);
    ok(pok && piv > 3.14 && piv < 3.15, QStringLiteral("PI ~3.14159 got %1").arg(wb.displayText(0, 70, 0)));

    wb.setRaw(0, 71, 0, QStringLiteral("=RAND()"));
    bool rok = false;
    const double rv = wb.displayText(0, 71, 0).toDouble(&rok);
    ok(rok && rv >= 0 && rv < 1, QStringLiteral("RAND in [0,1)"));

    wb.setRaw(0, 72, 0, QStringLiteral("=RANDBETWEEN(3,5)"));
    bool bok = false;
    const int rb = wb.displayText(0, 72, 0).toInt(&bok);
    ok(bok && rb >= 3 && rb <= 5, QStringLiteral("RANDBETWEEN 3..5"));

    const QString today = (wb.setRaw(0, 73, 0, QStringLiteral("=TODAY()")), wb.displayText(0, 73, 0));
    ok(today.size() == 10 && today[4] == QLatin1Char('-'), QStringLiteral("TODAY ISO got %1").arg(today));
    wb.setRaw(0, 74, 0, QStringLiteral("=NOW()"));
    ok(wb.displayText(0, 74, 0).contains(QLatin1Char('-')), QStringLiteral("NOW has date"));

    wb.setRaw(0, 0, 2, QStringLiteral("x"));
    wb.setRaw(0, 0, 3, QStringLiteral("7"));
    wb.setRaw(0, 1, 2, QStringLiteral("y"));
    wb.setRaw(0, 1, 3, QStringLiteral("9"));
    wb.setRaw(0, 75, 0, QStringLiteral("=VLOOKUP(\"y\",C1:D2,2,FALSE)"));
    eq(wb.displayText(0, 75, 0), QStringLiteral("9"), "VLOOKUP");

    wb.setRaw(0, 0, 4, QStringLiteral("a"));
    wb.setRaw(0, 0, 5, QStringLiteral("b"));
    wb.setRaw(0, 1, 4, QStringLiteral("1"));
    wb.setRaw(0, 1, 5, QStringLiteral("2"));
    wb.setRaw(0, 76, 0, QStringLiteral("=HLOOKUP(\"b\",E1:F2,2,FALSE)"));
    eq(wb.displayText(0, 76, 0), QStringLiteral("2"), "HLOOKUP");

    wb.setRaw(0, 77, 0, QStringLiteral("=INDEX(C1:D2,2,2)"));
    eq(wb.displayText(0, 77, 0), QStringLiteral("9"), "INDEX");
    wb.setRaw(0, 78, 0, QStringLiteral("=MATCH(\"y\",C1:C2,0)"));
    eq(wb.displayText(0, 78, 0), QStringLiteral("2"), "MATCH");

    wb.setRaw(0, 79, 0, QStringLiteral("=STDEV(A1:A3)"));
    bool sok = false;
    const double st = wb.displayText(0, 79, 0).toDouble(&sok);
    ok(sok && st > 9.9 && st < 10.1, QStringLiteral("STDEV of 10,20,30 = 10 got %1").arg(wb.displayText(0, 79, 0)));

    wb.setRaw(0, 0, 1, QStringLiteral("=B2"));
    wb.setRaw(0, 1, 1, QStringLiteral("=B1"));
    eq(wb.displayText(0, 0, 1), QStringLiteral("#CYCLE!"), "CYCLE");

    const QStringList deps = FormulaEngine::dependencyRefs(QStringLiteral("=SUM(B5:B15)"));
    ok(!deps.isEmpty(), QStringLiteral("xlfparser deps"));

    ok(FormulaEngine::catalog().size() >= 60, QStringLiteral("catalog size=%1").arg(FormulaEngine::catalog().size()));
    const QStringList cats = FormulaEngine::categories();
    ok(cats.contains(QStringLiteral("math")) && cats.contains(QStringLiteral("plugin")),
       QStringLiteral("categories include math+plugin"));

    QStringList names;
    for (const auto &info : FormulaEngine::catalog())
        names.append(info.name);
    ok(names.contains(QStringLiteral("SUM")) && names.contains(QStringLiteral("VLOOKUP")),
       QStringLiteral("catalog has SUM and VLOOKUP"));

    wb.addSheet(QStringLiteral("Данные"));
    wb.setRaw(1, 0, 1, QStringLiteral("7"));
    wb.setRaw(0, 80, 0, QStringLiteral("=Данные!B1"));
    eq(wb.displayText(0, 80, 0), QStringLiteral("7"), "cross sheet B1");
    wb.setRaw(0, 81, 0, QStringLiteral("=SIN(0)"));
    eq(wb.displayText(0, 81, 0), QStringLiteral("0"), "SIN 0");
    wb.setRaw(0, 82, 0, QStringLiteral("=SUBSTITUTE(\"aba\",\"a\",\"x\")"));
    eq(wb.displayText(0, 82, 0), QStringLiteral("xbx"), "SUBSTITUTE");
    wb.setRaw(0, 0, 6, QStringLiteral("10"));
    wb.setRaw(0, 1, 6, QStringLiteral("20"));
    wb.setRaw(0, 0, 7, QStringLiteral("a"));
    wb.setRaw(0, 1, 7, QStringLiteral("b"));
    wb.setRaw(0, 83, 0, QStringLiteral("=SUMIFS(G1:G2,H1:H2,\"a\")"));
    eq(wb.displayText(0, 83, 0), QStringLiteral("10"), "SUMIFS");
    wb.setRaw(0, 84, 0, QStringLiteral("=DATE(2020,1,15)+1"));
    eq(wb.displayText(0, 84, 0), QStringLiteral("2020-01-16"), "DATE+1");

    {
        Workbook huge;
        huge.setRaw(0, 0, 0, QStringLiteral("10"));
        huge.setRaw(0, 1, 0, QStringLiteral("20"));
        QElapsedTimer t;
        t.start();
        huge.setRaw(0, 0, 1, QStringLiteral("=SUM(A1:A1048576)"));
        eq(huge.displayText(0, 0, 1), QStringLiteral("30"), "SUM clipped to used rows");
        ok(t.elapsed() < 500, QStringLiteral("huge column range finished in %1 ms").arg(t.elapsed()));
        t.restart();
        huge.setRaw(0, 1, 1, QStringLiteral("=SUM(C1:XFD1048576)"));
        eq(huge.displayText(0, 1, 1), QStringLiteral("0"), "SUM empty huge range");
        ok(t.elapsed() < 500, QStringLiteral("whole-sheet range finished in %1 ms").arg(t.elapsed()));
    }
    {
        Workbook xs;
        xs.addSheet(QStringLiteral("Other"));
        xs.setRaw(1, 0, 0, QStringLiteral("5"));
        xs.setRaw(1, 1, 0, QStringLiteral("7"));
        xs.setRaw(0, 0, 0, QStringLiteral("100"));
        xs.setRaw(0, 1, 0, QStringLiteral("=SUM(Other!A1:A100)"));
        eq(xs.displayText(0, 1, 0), QStringLiteral("12"), "cross-sheet SUM range");
    }
    eq(CellRef::adjustFormula(QStringLiteral("=A1+1"), 1, 0), QStringLiteral("=A2+1"), "adjust relative");
    eq(CellRef::adjustFormula(QStringLiteral("=$A$1"), 1, 1), QStringLiteral("=$A$1"), "adjust abs");
    wb.setRaw(0, 90, 0, QStringLiteral("1"));
    wb.setRaw(0, 91, 0, QStringLiteral("=A91"));
    wb.insertRows(0, 90, 1);
    eq(wb.sheet(0).cell(92, 0).raw, QStringLiteral("=A92"), "insert row shifted formula");
    wb.undoStack()->undo();
    eq(wb.sheet(0).cell(90, 0).raw, QStringLiteral("1"), "undo insert row");
    eq(wb.sheet(0).cell(91, 0).raw, QStringLiteral("=A91"), "undo insert restored formula");

    wb.setRaw(0, 100, 0, QStringLiteral("10"));
    wb.setRaw(0, 101, 0, QStringLiteral("20"));
    wb.fill(0, 100, 0, 101, 0, 100, 0, 103, 0);
    eq(wb.sheet(0).cell(102, 0).raw, QStringLiteral("30"), "fill series 10,20 -> 30");
    eq(wb.sheet(0).cell(103, 0).raw, QStringLiteral("40"), "fill series 10,20 -> 40");
    wb.undoStack()->undo();
    eq(wb.sheet(0).cell(102, 0).raw, QString(), "undo fill");
    wb.setRaw(0, 100, 1, QStringLiteral("5"));
    wb.fill(0, 100, 1, 100, 1, 100, 1, 102, 1);
    eq(wb.sheet(0).cell(101, 1).raw, QStringLiteral("6"), "fill single number +1");
    wb.setRaw(0, 100, 2, QStringLiteral("=A101"));
    wb.fill(0, 100, 2, 100, 2, 100, 2, 102, 2);
    eq(wb.sheet(0).cell(101, 2).raw, QStringLiteral("=A102"), "fill formula relative");

    wb.setRaw(0, 110, 0, QStringLiteral("arbuzfind"));
    wb.setRaw(0, 112, 0, QStringLiteral("arbuzfind"));
    int fr = -1, fc = -1;
    ok(wb.findNext(0, QStringLiteral("arbuzfind"), 110, 0, &fr, &fc, true) && fr == 112 && fc == 0,
       QStringLiteral("find next"));
    ok(wb.findNext(0, QStringLiteral("arbuzfind"), 112, 0, &fr, &fc, true) && fr == 110 && fc == 0,
       QStringLiteral("find next wraps"));
    ok(wb.findPrev(0, QStringLiteral("arbuzfind"), 112, 0, &fr, &fc, true) && fr == 110 && fc == 0,
       QStringLiteral("find prev"));
    ok(wb.findPrev(0, QStringLiteral("arbuzfind"), 110, 0, &fr, &fc, true) && fr == 112 && fc == 0,
       QStringLiteral("find prev wraps"));
    ok(!wb.findNext(0, QStringLiteral("zzznomatch"), 0, -1, &fr, &fc, true), QStringLiteral("find miss"));

    {
        Workbook n;
        CellData d;
        d.raw = QStringLiteral("1234.5");
        d.numFmt = NumFormat::Number2;
        n.setCellData(0, 0, 0, d);
        eq(n.displayText(0, 0, 0), QStringLiteral("1234.50"), "num 0.00");
        d.numFmt = NumFormat::Thousands2;
        n.setCellData(0, 0, 0, d);
        eq(n.displayText(0, 0, 0), QStringLiteral("1,234.50"), "num thousands");
        d.raw = QStringLiteral("0.25");
        d.numFmt = NumFormat::Percent;
        n.setCellData(0, 0, 0, d);
        eq(n.displayText(0, 0, 0), QStringLiteral("25%"), "num percent");
        d.numFmt = NumFormat::Percent2;
        n.setCellData(0, 0, 0, d);
        eq(n.displayText(0, 0, 0), QStringLiteral("25.00%"), "num percent 2");
        d.raw = QStringLiteral("1000");
        d.numFmt = NumFormat::Scientific;
        n.setCellData(0, 0, 0, d);
        ok(n.displayText(0, 0, 0).contains(QLatin1Char('E')),
           QStringLiteral("num scientific got %1").arg(n.displayText(0, 0, 0)));
        n.setRaw(0, 2, 0, QStringLiteral("a"));
        n.setRaw(0, 0, 2, QStringLiteral("c"));
        int ur = -1, uc = -1;
        ok(n.usedCorner(0, &ur, &uc) && ur == 2 && uc == 2, QStringLiteral("used corner C3"));
    }

    {
        Workbook c;
        double n = 0;
        ok(NumFormat::parse(QStringLiteral("1,5"), &n) && qAbs(n - 1.5) < 1e-12, QStringLiteral("parse 1,5"));
        ok(NumFormat::parse(QStringLiteral("1 234,56"), &n) && qAbs(n - 1234.56) < 1e-9,
           QStringLiteral("parse 1 234,56"));
        ok(NumFormat::parse(QStringLiteral("1,234.50"), &n) && qAbs(n - 1234.5) < 1e-9,
           QStringLiteral("parse 1,234.50"));
        ok(NumFormat::parse(QStringLiteral("25%"), &n) && qAbs(n - 0.25) < 1e-12, QStringLiteral("parse 25%"));
        ok(!NumFormat::parse(QStringLiteral("abc"), &n), QStringLiteral("parse reject text"));

        c.setRaw(0, 0, 0, QStringLiteral("1,5"));
        c.setRaw(0, 1, 0, QStringLiteral("=A1*2"));
        eq(c.displayText(0, 1, 0), QStringLiteral("3"), "cell 1,5 * 2");
        c.setRaw(0, 2, 0, QStringLiteral("=1,5+2"));
        eq(c.displayText(0, 2, 0), QStringLiteral("3.5"), "formula 1,5+2");
        c.setRaw(0, 3, 0, QStringLiteral("=A1*0,13"));
        eq(c.displayText(0, 3, 0), QStringLiteral("0.195"), "rate 0,13");
        c.setRaw(0, 4, 0, QStringLiteral("=VALUE(\"1,5\")"));
        eq(c.displayText(0, 4, 0), QStringLiteral("1.5"), "VALUE 1,5");
        c.setRaw(0, 5, 0, QStringLiteral("=POWER(2,10)"));
        eq(c.displayText(0, 5, 0), QStringLiteral("1024"), "POWER comma args");
        c.setRaw(0, 6, 0, QStringLiteral("25%"));
        c.setRaw(0, 7, 0, QStringLiteral("=A7*200"));
        eq(c.displayText(0, 7, 0), QStringLiteral("50"), "25% * 200");
        c.setRaw(0, 8, 0, QStringLiteral("1,5"));
        c.setRaw(0, 9, 0, QStringLiteral("2,5"));
        c.setRaw(0, 10, 0, QStringLiteral("=SUM(A9:A10)"));
        eq(c.displayText(0, 10, 0), QStringLiteral("4"), "SUM 1,5+2,5");
        c.fill(0, 8, 0, 8, 0, 8, 0, 9, 0);
        eq(c.sheet(0).cell(9, 0).raw, QStringLiteral("2.5"), "fill 1,5 -> 2.5");
    }
}

static void testSheetModel()
{
    Workbook wb;
    wb.setRaw(0, 0, 0, QStringLiteral("42"));
    SheetModel model(&wb, 0);
    ok(model.rowCount() >= 50 && model.columnCount() >= 10, QStringLiteral("model size"));
    eq(model.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("42"), "model display");
    ok(model.setData(model.index(1, 0), QStringLiteral("=A1*2"), Qt::EditRole), QStringLiteral("model edit"));
    eq(model.index(1, 0).data(Qt::DisplayRole).toString(), QStringLiteral("84"), "model formula");
    eq(model.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QStringLiteral("A"), "header A");
    eq(model.headerData(0, Qt::Vertical, Qt::DisplayRole).toString(), QStringLiteral("1"), "header 1");
}

static void testFileIo()
{
    Workbook wb;
    wb.setRaw(0, 0, 0, QStringLiteral("10"));
    wb.setRaw(0, 1, 0, QStringLiteral("=A1+5"));
    wb.addSheet(QStringLiteral("Второй"));
    wb.setRaw(1, 0, 0, QStringLiteral("привет"));
    QTemporaryDir tmp;
    const QString xlsx = tmp.filePath(QStringLiteral("t.xlsx"));
    const QString csv = tmp.filePath(QStringLiteral("t.csv"));
    QString err;
    wb.setStyle(0, 0, 0, true, false, QColor("#112233"), QColor("#abcdef"));
    ok(FileIo::save(&wb, xlsx, &err), QStringLiteral("save xlsx %1").arg(err));
    Workbook loaded;
    ok(FileIo::load(&loaded, xlsx, &err), QStringLiteral("load xlsx %1").arg(err));
    ok(loaded.sheetCount() >= 2, QStringLiteral("xlsx sheets=%1").arg(loaded.sheetCount()));
    eq(loaded.displayText(0, 1, 0), QStringLiteral("15"), "xlsx formula");
    eq(loaded.displayText(1, 0, 0), QStringLiteral("привет"), "xlsx unicode sheet2");
    ok(loaded.sheet(0).cell(0, 0).bold, QStringLiteral("xlsx style bold roundtrip"));
    {
        CellData d;
        d.raw = QStringLiteral("12.5");
        d.numFmt = NumFormat::Number2;
        loaded.setCellData(0, 2, 0, d);
    }
    const QString xlsxFmt = tmp.filePath(QStringLiteral("fmt.xlsx"));
    ok(FileIo::save(&loaded, xlsxFmt, &err), QStringLiteral("save xlsx numfmt %1").arg(err));
    Workbook fmtWb;
    ok(FileIo::load(&fmtWb, xlsxFmt, &err), QStringLiteral("load xlsx numfmt %1").arg(err));
    eq(fmtWb.displayText(0, 2, 0), QStringLiteral("12.50"), "xlsx num 0.00 roundtrip");
    ok(FileIo::save(&wb, csv, &err), QStringLiteral("save csv"));
    Workbook csvWb;
    ok(FileIo::load(&csvWb, csv, &err), QStringLiteral("load csv"));
    ok(csvWb.displayText(0, 0, 0).contains(QLatin1String("10")), QStringLiteral("csv has 10"));

    QFile quoted(tmp.filePath(QStringLiteral("q.csv")));
    ok(quoted.open(QIODevice::WriteOnly | QIODevice::Text), QStringLiteral("write quoted csv"));
    quoted.write("\"a,b\",2\n");
    quoted.close();
    Workbook qwb;
    ok(FileIo::load(&qwb, quoted.fileName(), &err), QStringLiteral("load quoted csv"));
    eq(qwb.displayText(0, 0, 0), QStringLiteral("a,b"), "csv quoted comma");
}

static QString colorKey(const QColor &c)
{
    if (!c.isValid())
        return QStringLiteral("-");
    return c.name(QColor::HexRgb).toLower();
}

static bool cellContractEq(const CellData &a, const CellData &b, QString *why)
{
    auto fail = [&](const char *field, const QString &left, const QString &right) {
        *why = QStringLiteral("%1: '%2' vs '%3'").arg(QLatin1String(field), left, right);
        return false;
    };
    if (a.raw != b.raw)
        return fail("raw", a.raw, b.raw);
    if (a.bold != b.bold)
        return fail("bold", a.bold ? QStringLiteral("1") : QStringLiteral("0"),
                    b.bold ? QStringLiteral("1") : QStringLiteral("0"));
    if (a.italic != b.italic)
        return fail("italic", a.italic ? QStringLiteral("1") : QStringLiteral("0"),
                    b.italic ? QStringLiteral("1") : QStringLiteral("0"));
    if (colorKey(a.foreground) != colorKey(b.foreground))
        return fail("fg", colorKey(a.foreground), colorKey(b.foreground));
    if (colorKey(a.background) != colorKey(b.background))
        return fail("bg", colorKey(a.background), colorKey(b.background));
    if (a.hAlign != b.hAlign)
        return fail("hAlign", QString::number(a.hAlign), QString::number(b.hAlign));
    if (a.vAlign != b.vAlign)
        return fail("vAlign", QString::number(a.vAlign), QString::number(b.vAlign));
    if (a.wrap != b.wrap)
        return fail("wrap", a.wrap ? QStringLiteral("1") : QStringLiteral("0"),
                    b.wrap ? QStringLiteral("1") : QStringLiteral("0"));
    if (a.numFmt != b.numFmt)
        return fail("numFmt", QString::number(a.numFmt), QString::number(b.numFmt));
    if (a.border != b.border)
        return fail("border", QString::number(a.border), QString::number(b.border));
    return true;
}

static void checkOwnedXlsx(const Workbook &src, const Workbook &got, const char *tag)
{
    ok(got.sheetCount() == src.sheetCount(),
       QStringLiteral("%1 sheets %2 vs %3").arg(QLatin1String(tag)).arg(got.sheetCount()).arg(src.sheetCount()));
    const int n = qMin(src.sheetCount(), got.sheetCount());
    for (int s = 0; s < n; ++s) {
        const Worksheet &a = src.sheet(s);
        const Worksheet &b = got.sheet(s);
        ok(b.name == a.name,
           QStringLiteral("%1 sheet%2 name got '%3' want '%4'")
               .arg(QLatin1String(tag))
               .arg(s)
               .arg(b.name, a.name));
        ok(b.freezeRows == a.freezeRows && b.freezeCols == a.freezeCols,
           QStringLiteral("%1 sheet%2 freeze %3,%4 vs %5,%6")
               .arg(QLatin1String(tag))
               .arg(s)
               .arg(b.freezeRows)
               .arg(b.freezeCols)
               .arg(a.freezeRows)
               .arg(a.freezeCols));
        ok(b.merges.size() == a.merges.size(),
           QStringLiteral("%1 sheet%2 merges %3 vs %4")
               .arg(QLatin1String(tag))
               .arg(s)
               .arg(b.merges.size())
               .arg(a.merges.size()));
        const int mc = qMin(a.merges.size(), b.merges.size());
        for (int i = 0; i < mc; ++i) {
            const MergeRange &ma = a.merges.at(i);
            const MergeRange &mb = b.merges.at(i);
            ok(ma.r1 == mb.r1 && ma.c1 == mb.c1 && ma.r2 == mb.r2 && ma.c2 == mb.c2,
               QStringLiteral("%1 sheet%2 merge %3").arg(QLatin1String(tag)).arg(s).arg(i));
        }
        for (auto it = a.columnWidths.cbegin(); it != a.columnWidths.cend(); ++it) {
            ok(b.columnWidths.value(it.key()) == it.value(),
               QStringLiteral("%1 sheet%2 colW %3 got %4 want %5")
                   .arg(QLatin1String(tag))
                   .arg(s)
                   .arg(it.key())
                   .arg(b.columnWidths.value(it.key()))
                   .arg(it.value()));
        }
        QSet<quint64> keys;
        for (auto it = a.cells.cbegin(); it != a.cells.cend(); ++it)
            keys.insert(it.key());
        for (auto it = b.cells.cbegin(); it != b.cells.cend(); ++it)
            keys.insert(it.key());
        for (quint64 k : keys) {
            const int row = int(k >> 32);
            const int col = int(k & 0xffffffffu);
            QString why;
            ok(cellContractEq(a.cell(row, col), b.cell(row, col), &why),
               QStringLiteral("%1 %2!%3 %4")
                   .arg(QLatin1String(tag), a.name, CellRef::a1(row, col), why));
        }
    }
}

static void testXlsxContract()
{
    Workbook src;
    src.setRaw(0, 0, 0, QStringLiteral("10"));
    src.setRaw(0, 1, 0, QStringLiteral("=A1+5"));
    src.setRaw(0, 2, 0, QStringLiteral("=Второй!A1"));
    src.setStyle(0, 0, 0, true, true, QColor(QStringLiteral("#112233")), QColor(QStringLiteral("#abcdef")));
    CellData pct;
    pct.raw = QStringLiteral("0.25");
    pct.numFmt = NumFormat::Percent;
    pct.hAlign = 3;
    pct.wrap = true;
    pct.border = 15;
    src.setCellData(0, 0, 1, pct);
    CellData fillOnly;
    fillOnly.background = QColor(QStringLiteral("#ffcc00"));
    src.setCellData(0, 4, 2, fillOnly);
    src.sheet(0).columnWidths.insert(0, 96);
    src.sheet(0).columnWidths.insert(2, 72);
    src.setFreeze(0, 2, 1);
    src.mergeCells(0, 3, 0, 3, 1);

    src.addSheet(QStringLiteral("Второй"));
    src.setRaw(1, 0, 0, QStringLiteral("привет"));
    src.setRaw(1, 1, 0, QStringLiteral("=Лист1!A1*2"));

    src.addSheet(QStringLiteral("Мой лист"));
    src.setRaw(2, 0, 0, QStringLiteral("7"));
    src.setRaw(0, 5, 0, QStringLiteral("='Мой лист'!A1"));

    QTemporaryDir tmp;
    const QString path = tmp.filePath(QStringLiteral("contract.xlsx"));
    QString err;
    ok(FileIo::save(&src, path, &err), QStringLiteral("contract save %1").arg(err));

    Workbook once;
    ok(FileIo::load(&once, path, &err), QStringLiteral("contract load %1").arg(err));
    checkOwnedXlsx(src, once, "xlsx1");
    eq(once.displayText(0, 1, 0), QStringLiteral("15"), "contract formula A2");
    eq(once.displayText(0, 2, 0), QStringLiteral("привет"), "contract cross sheet");
    eq(once.displayText(1, 1, 0), QStringLiteral("20"), "contract back-ref");
    eq(once.displayText(0, 5, 0), QStringLiteral("7"), "contract quoted sheet");
    eq(once.displayText(0, 0, 1), QStringLiteral("25%"), "contract percent display");

    const QString path2 = tmp.filePath(QStringLiteral("contract2.xlsx"));
    ok(FileIo::save(&once, path2, &err), QStringLiteral("contract resave %1").arg(err));
    Workbook twice;
    ok(FileIo::load(&twice, path2, &err), QStringLiteral("contract reload %1").arg(err));
    checkOwnedXlsx(src, twice, "xlsx2");
    eq(twice.displayText(0, 2, 0), QStringLiteral("привет"), "contract 2nd pass cross sheet");
}

static void testThemeAndI18n()
{
    ok(Theme::builtinIds().contains(QStringLiteral("white"))
           && Theme::builtinIds().contains(QStringLiteral("dark"))
           && Theme::builtinIds().contains(QStringLiteral("arbuz")),
       QStringLiteral("builtin themes"));
    const auto pal = Theme::paletteFor(QStringLiteral("arbuz"));
    ok(pal.contains(QStringLiteral("seed")) && pal.contains(QStringLiteral("pith")),
       QStringLiteral("arbuz palette roles"));
    const QString ss = Theme::stylesheet(pal);
    ok(ss.contains(QStringLiteral("QMainWindow")) && ss.contains(QLatin1Char('#')),
       QStringLiteral("stylesheet generated"));
    eq(Theme::idFromTitle(QStringLiteral("Моя тема")), QStringLiteral("моя-тема"), "idFromTitle");

    {
        QTemporaryDir tmp;
        qputenv("ARBUZ_PORTABLE_ROOT", tmp.path().toUtf8());
        const QString themes = Theme::userThemesDir();
        ok(themes.startsWith(tmp.path()) && themes.endsWith(QStringLiteral("themes")),
           QStringLiteral("portable themes dir"));
        qunsetenv("ARBUZ_PORTABLE_ROOT");
    }

    const QString old = I18n::lang();
    I18n::setLang(QStringLiteral("en"));
    eq(I18n::t("ui.file"), QStringLiteral("File"), "i18n en");
    I18n::setLang(QStringLiteral("ru"));
    eq(I18n::t("ui.file"), QStringLiteral("Файл"), "i18n ru");
    ok(I18n::creditsId() == QLatin1String("ru"), QStringLiteral("credits id ru"));
    ok(I18n::creditsMarkdown().contains(QStringLiteral("табличный")),
       QStringLiteral("credits markdown ru"));
    I18n::setLang(QStringLiteral("en"));
    ok(I18n::creditsId() == QLatin1String("en"), QStringLiteral("credits id en"));
    ok(I18n::creditsMarkdown().contains(QStringLiteral("table calculator")),
       QStringLiteral("credits markdown en"));
    I18n::setLang(old);
}

static void testIcons()
{
    const QIcon ic = ArbuzIcon::app();
    ok(!ic.isNull(), QStringLiteral("app icon"));
    const QPixmap px = ic.pixmap(64, 64);
    ok(!px.isNull() && px.width() >= 16, QStringLiteral("app pixmap"));
    const QColor mid = px.toImage().pixelColor(px.width() / 2, px.height() / 2);
    ok(mid.red() > 80 && mid.red() > mid.green(), QStringLiteral("icon flesh is red, rgb=%1,%2,%3")
                                                      .arg(mid.red())
                                                      .arg(mid.green())
                                                      .arg(mid.blue()));
    ok(mid.lightness() > 20, QStringLiteral("icon center is not a black seed"));
    ok(!ArbuzIcon::named(QStringLiteral("fx")).isNull(), QStringLiteral("fx icon"));
    ok(!ArbuzIcon::addSheet().isNull(), QStringLiteral("add-sheet icon"));
}

static void testPluginBridge()
{
    FormulaEngine::FormulaInfo info;
    info.name = QStringLiteral("DOUBLE");
    info.syntax = QStringLiteral("DOUBLE(n)");
    info.category = QStringLiteral("plugin");
    info.helpRuOverride = QStringLiteral("удвоение");
    FormulaEngine::setPluginBridge(
        [](const QString &name, const QVector<FormulaArg> &args) {
            if (name != QLatin1String("DOUBLE") || args.isEmpty())
                return FormulaValue::fromError(QStringLiteral("#NAME?"));
            return FormulaValue::fromNumber(args.first().value.asNumber() * 2);
        },
        {info});
    ok(FormulaEngine::pluginCatalog().size() == 1, QStringLiteral("plugin catalog"));
    Workbook wb;
    wb.setRaw(0, 0, 0, QStringLiteral("=DOUBLE(21)"));
    eq(wb.displayText(0, 0, 0), QStringLiteral("42"), "plugin DOUBLE");
    FormulaEngine::setPluginBridge({}, {});
}

static void testPythonPlugins()
{
    QTemporaryDir tmp;
    const QString plug = tmp.filePath(QStringLiteral("plugins/probe"));
    QDir().mkpath(plug);
    QFile f(plug + QStringLiteral("/plugin.py"));
    ok(f.open(QIODevice::WriteOnly | QIODevice::Text), QStringLiteral("write probe plugin"));
    f.write("import arbuz\n"
            "@arbuz.function('VAT', syntax='VAT(amount)', help_ru='nds')\n"
            "def vat(amount):\n"
            "    nums = arbuz.numbers(amount)\n"
            "    return (nums[0] if nums else 0) * 0.20\n"
            "@arbuz.function('FIRST', syntax='FIRST(range)')\n"
            "def first(arg):\n"
            "    nums = arbuz.numbers(arg)\n"
            "    return nums[0] if nums else 0\n"
            "@arbuz.command('probe.fill', 'Заполнить B1', 'Fill B1')\n"
            "def fill():\n"
            "    arbuz.set('B1', 'ok')\n");
    f.close();

    qputenv("ARBUZ_PORTABLE_ROOT", tmp.path().toUtf8());
    qputenv("ARBUZ_NO_PLUGINS", "0");

    Workbook wb;
    PluginHost::instance().start(&wb);
    if (!PluginHost::instance().running()) {
        fail(QStringLiteral("python plugin host not running: %1 python=%2")
                 .arg(PluginHost::instance().statusMessage(), PluginHost::instance().pythonPath()));
        qunsetenv("ARBUZ_PORTABLE_ROOT");
        return;
    }
    ok(PluginHost::instance().loadedPlugins().contains(QStringLiteral("probe")),
       QStringLiteral("loaded probe plugin: %1").arg(PluginHost::instance().loadedPlugins().join(',')));
    wb.setRaw(0, 0, 0, QStringLiteral("100"));
    wb.setRaw(0, 1, 0, QStringLiteral("=VAT(A1)"));
    eq(wb.displayText(0, 1, 0), QStringLiteral("20"), "VAT(A1)");
    wb.setRaw(0, 2, 0, QStringLiteral("=VAT(50)"));
    eq(wb.displayText(0, 2, 0), QStringLiteral("10"), "VAT(50)");

    wb.addSheet(QStringLiteral("Other"));
    wb.setRaw(1, 0, 0, QStringLiteral("99"));
    wb.setRaw(1, 1, 0, QStringLiteral("1"));
    PluginHost::instance().setCurrentSheet(0);
    wb.setRaw(0, 3, 0, QStringLiteral("=FIRST(Other!A1:A2)"));
    eq(wb.displayText(0, 3, 0), QStringLiteral("99"), "plugin FIRST other-sheet range");

    QMenu menu;
    PluginHost::instance().fillMenu(&menu);
    QAction *fill = nullptr;
    for (QAction *a : menu.actions()) {
        if (a->text().contains(QStringLiteral("Заполнить")) || a->text().contains(QStringLiteral("Fill B1")))
            fill = a;
    }
    ok(fill != nullptr, QStringLiteral("plugin command in menu"));
    if (fill)
        fill->trigger();
    eq(wb.displayText(0, 0, 1), QStringLiteral("ok"), "command wrote B1");

    PluginHost::instance().stop();
    qunsetenv("ARBUZ_PORTABLE_ROOT");
    qputenv("ARBUZ_NO_PLUGINS", "1");
}

static void testNewFeatures()
{
    {
        QString f = QStringLiteral("=A1+B2");
        int pos = 2;
        f = CellRef::cycleReferenceAt(f, pos, &pos);
        ok(f.contains(QStringLiteral("$A$1")), QStringLiteral("F4 cycle to $A$1"));
        f = CellRef::cycleReferenceAt(f, pos, &pos);
        ok(f.contains(QStringLiteral("A$1")), QStringLiteral("F4 cycle to A$1"));
        f = CellRef::cycleReferenceAt(f, pos, &pos);
        ok(f.contains(QStringLiteral("$A1")), QStringLiteral("F4 cycle to $A1"));
        f = CellRef::cycleReferenceAt(f, pos, &pos);
        ok(f.contains(QStringLiteral("A1")) && !f.contains(QLatin1Char('$')),
           QStringLiteral("F4 cycle back to A1"));
    }

    {
        CellData src;
        src.raw = QStringLiteral("=A1+1");
        src.bold = true;
        src.background = QColor(QStringLiteral("#ffeecc"));
        src.numFmt = NumFormat::CurrencyRub;
        const QVector<CellData> block{src};
        std::unique_ptr<QMimeData> mime(ClipData::mimeFromBlock(1, 1, block, src.raw));
        int rows = 0;
        int cols = 0;
        QVector<CellData> out;
        QString tsv;
        ok(ClipData::blockFromMime(mime.get(), &rows, &cols, &out, &tsv), QStringLiteral("clip mime parse"));
        ok(rows == 1 && cols == 1 && out.size() == 1, QStringLiteral("clip block size"));
        ok(out.at(0).bold && out.at(0).numFmt == NumFormat::CurrencyRub, QStringLiteral("clip keeps style"));
        ok(out.at(0).background == src.background, QStringLiteral("clip keeps fill"));
        const CellData pasted = ClipData::cellForPaste(out.at(0), 2, 1);
        eq(pasted.raw, QStringLiteral("=B3+1"), "clip paste adjusts formula");
    }

    {
        Workbook wb;
        wb.addSheet(QStringLiteral("Two"));
        wb.setRaw(0, 0, 0, QStringLiteral("alpha"));
        wb.setRaw(0, 0, 1, QStringLiteral("beta"));
        wb.setRaw(1, 0, 0, QStringLiteral("gamma"));
        int sh = 0;
        int r = 0;
        int c = 0;
        ok(wb.findNextInWorkbook(0, 0, -1, QStringLiteral("beta"), &sh, &r, &c, false),
           QStringLiteral("workbook find sheet0"));
        ok(sh == 0 && r == 0 && c == 1, QStringLiteral("workbook find beta coords"));
        ok(wb.findNextInWorkbook(sh, r, c, QStringLiteral("gamma"), &sh, &r, &c, false),
           QStringLiteral("workbook find next sheet"));
        ok(sh == 1 && r == 0 && c == 0, QStringLiteral("workbook find gamma on sheet1"));
        ok(wb.findPrevInWorkbook(sh, r, c, QStringLiteral("beta"), &sh, &r, &c, false),
           QStringLiteral("workbook find prev"));
        ok(sh == 0 && r == 0 && c == 1, QStringLiteral("workbook find prev beta"));
    }

    {
        Workbook wb;
        wb.setRaw(0, 0, 0, QStringLiteral("Item"));
        wb.setRaw(0, 0, 1, QStringLiteral("Qty"));
        wb.setRaw(0, 1, 0, QStringLiteral("Apple"));
        wb.setRaw(0, 1, 1, QStringLiteral("3"));
        wb.setRaw(0, 2, 0, QStringLiteral("Banana"));
        wb.setRaw(0, 2, 1, QStringLiteral("5"));
        wb.setAutoFilter(0, 0, 0, 1, 1, 2);
        wb.setAutoFilterCriteria(0, 0, QStringLiteral("Apple"));
        ok(wb.rowVisibleWithFilter(0, 0), QStringLiteral("autofilter header visible"));
        ok(wb.rowVisibleWithFilter(0, 1), QStringLiteral("autofilter apple row visible"));
        ok(!wb.rowVisibleWithFilter(0, 2), QStringLiteral("autofilter banana row hidden"));
        wb.clearAutoFilter(0);
        ok(wb.rowVisibleWithFilter(0, 2), QStringLiteral("autofilter clear shows all"));
    }

    eq(NumFormat::format(1234.5, NumFormat::CurrencyRub), QStringLiteral("1,234.50 ₽"), "currency rub");
    eq(NumFormat::format(99.9, NumFormat::CurrencyUsd), QStringLiteral("$99.90"), "currency usd");
    eq(NumFormat::format(42.0, NumFormat::CurrencyEur), QStringLiteral("42.00 €"), "currency eur");

    QTemporaryDir tmp;
    Workbook wb;
    wb.setRaw(0, 0, 0, QStringLiteral("=1+2"));
    wb.setRaw(0, 1, 0, QStringLiteral("plain"));
    wb.sheet(0).rowHeights.insert(2, 36);
    QString err;
    const QString csv = tmp.filePath(QStringLiteral("formulas.csv"));
    ok(FileIo::save(&wb, csv, &err), QStringLiteral("save csv formulas %1").arg(err));
    QFile cf(csv);
    ok(cf.open(QIODevice::ReadOnly | QIODevice::Text), QStringLiteral("open csv"));
    const QString csvBody = QString::fromUtf8(cf.readAll());
    ok(csvBody.contains(QStringLiteral("=1+2")), QStringLiteral("csv keeps formula text"));
    Workbook csvWb;
    ok(FileIo::load(&csvWb, csv, &err), QStringLiteral("reload csv formulas"));
    eq(csvWb.sheet(0).cell(0, 0).raw, QStringLiteral("=1+2"), "csv formula roundtrip raw");

    const QString xlsx = tmp.filePath(QStringLiteral("rows.xlsx"));
    ok(FileIo::save(&wb, xlsx, &err), QStringLiteral("save xlsx row heights %1").arg(err));
    Workbook xlsxWb;
    ok(FileIo::load(&xlsxWb, xlsx, &err), QStringLiteral("load xlsx row heights %1").arg(err));
    ok(xlsxWb.sheet(0).rowHeights.value(2) == 36, QStringLiteral("xlsx row height roundtrip"));
    eq(xlsxWb.displayText(0, 0, 0), QStringLiteral("3"), "xlsx formula still evaluates after deflate");
}

int runSelfTest()
{
    g_fails = 0;
    g_ok = 0;
    // Contract and UI strings in this suite are Russian-locale fixtures.
    I18n::setLang(QStringLiteral("ru"));
    testCellRef();
    testWorkbookSheets();
    testFormulas();
    testSheetModel();
    testFileIo();
    testXlsxContract();
    testNewFeatures();
    testThemeAndI18n();
    testIcons();
    testPluginBridge();
    testPythonPlugins();
    if (g_fails == 0)
        std::printf("self-test ok (%d checks)\n", g_ok);
    else
        std::fprintf(stderr, "self-test finished with %d failure(s), %d ok\n", g_fails, g_ok);
    return g_fails == 0 ? 0 : 1;
}
