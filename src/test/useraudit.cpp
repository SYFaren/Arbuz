#include "useraudit.h"

#include "cellref.h"
#include "chart.h"
#include "clipdata.h"
#include "demo.h"
#include "fileio.h"
#include "i18n.h"
#include "numformat.h"
#include "workbook.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMimeData>
#include <QTemporaryDir>
#include <cstdio>

static int g_fails = 0;
static int g_ok = 0;

static void fail(const QString &msg)
{
    std::fprintf(stderr, "USER FAIL: %s\n", qPrintable(msg));
    ++g_fails;
}

static void ok(bool cond, const QString &msg)
{
    if (!cond)
        fail(msg);
    else
        ++g_ok;
}

static void eq(const QString &got, const QString &want, const char *name)
{
    if (got != want)
        fail(QStringLiteral("%1: got '%2' expected '%3'").arg(QLatin1String(name), got, want));
    else
        ++g_ok;
}

static void near(double got, double want, const char *name, double eps = 0.01)
{
    if (qAbs(got - want) > eps)
        fail(QStringLiteral("%1: got %2 expected ~%3").arg(QLatin1String(name)).arg(got).arg(want));
    else
        ++g_ok;
}

static int sheetIndex(const Workbook &wb, const QString &name)
{
    for (int i = 0; i < wb.sheetCount(); ++i) {
        if (wb.sheet(i).name == name)
            return i;
    }
    return -1;
}

static void auditDemoFormulas(Workbook *wb)
{
    const int sh = sheetIndex(*wb, QStringLiteral("Formulas"));
    ok(sh >= 0, QStringLiteral("Formulas sheet exists"));
    if (sh < 0)
        return;
    wb->recalculate();

    near(wb->displayText(sh, 1, 0).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 1.5, "demo A2 decimal");
    near(wb->displayText(sh, 2, 0).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 2.5, "demo A3 decimal");
    near(wb->displayText(sh, 3, 0).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 4.0, "demo SUM");
    near(wb->displayText(sh, 2, 1).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 3.0, "demo A2*2");
    near(wb->displayText(sh, 1, 2).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 15.0, "demo cross self-ref");
    eq(wb->displayText(sh, 2, 2), QStringLiteral("да"), "demo IF");
    eq(wb->displayText(sh, 3, 2), QStringLiteral("50"), "demo VLOOKUP");
    ok(wb->displayText(sh, 1, 3).size() == 10, QStringLiteral("demo TODAY iso len"));
    near(wb->displayText(sh, 2, 3).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 2.0, "demo AVERAGE");

    int fr = 0, fc = 0;
    ok(wb->findNext(sh, QStringLiteral("FINDME"), 0, -1, &fr, &fc, false),
       QStringLiteral("demo find FINDME on Formulas"));
    ok(fr == 10, QStringLiteral("demo FINDME row=%1").arg(fr));
}

static void auditDemoCross(Workbook *wb)
{
    const int sh = sheetIndex(*wb, QStringLiteral("Cross"));
    ok(sh >= 0, QStringLiteral("Cross sheet"));
    if (sh < 0)
        return;
    near(wb->displayText(sh, 0, 1).toDouble(), 42.0, "demo Cross B1");
    near(wb->displayText(sh, 1, 0).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 1.5, "demo Cross Formulas!A2");
    near(wb->displayText(sh, 2, 0).toDouble(), 84.0, "demo Cross B1*2");
}

static void auditDemoData(Workbook *wb)
{
    const int sh = sheetIndex(*wb, QStringLiteral("Data"));
    ok(sh >= 0, QStringLiteral("Data sheet"));
    if (sh < 0)
        return;
    eq(wb->displayText(sh, 8, 0), QStringLiteral("29"), "demo SUM qty");
    ok(!wb->sheet(sh).cell(1, 0).raw.isEmpty(), QStringLiteral("data row before sort"));

    wb->setAutoFilter(sh, 0, 0, 3, 1, 5);
    wb->setAutoFilterCriteria(sh, 1, QStringLiteral("Яблоки"));
    ok(wb->rowVisibleWithFilter(sh, 1), QStringLiteral("demo filter apple row1"));
    ok(wb->rowVisibleWithFilter(sh, 3), QStringLiteral("demo filter apple row3"));
    ok(!wb->rowVisibleWithFilter(sh, 2), QStringLiteral("demo filter hides pears"));
    wb->clearAutoFilter(sh);

    wb->sortRange(sh, 1, 0, 5, 3, 0, true);
    eq(wb->sheet(sh).cell(1, 0).raw, QStringLiteral("Магазин"), "demo sort first dept");
    eq(wb->sheet(sh).cell(5, 0).raw, QStringLiteral("Склад"), "demo sort last dept");
}

static void auditDemoFormats(Workbook *wb)
{
    const int sh = sheetIndex(*wb, QStringLiteral("Formats"));
    ok(sh >= 0, QStringLiteral("Formats sheet"));
    if (sh < 0)
        return;
    ok(wb->sheet(sh).cell(1, 1).bold, QStringLiteral("demo bold"));
    ok(wb->sheet(sh).cell(2, 1).italic, QStringLiteral("demo italic"));
    ok(NumFormat::format(1234.567, wb->sheet(sh).cell(8, 1).numFmt).contains(QStringLiteral("1")),
       QStringLiteral("demo number format"));
    ok(NumFormat::format(0.25, wb->sheet(sh).cell(9, 1).numFmt).contains(QLatin1Char('%')),
       QStringLiteral("demo percent format"));

    CellData src = wb->sheet(sh).cell(13, 0);
    const QVector<CellData> block{src};
    std::unique_ptr<QMimeData> mime(ClipData::mimeFromBlock(1, 1, block, src.raw));
    QVector<CellData> out;
    int rows = 0, cols = 0;
    QString tsv;
    ok(ClipData::blockFromMime(mime.get(), &rows, &cols, &out, &tsv), QStringLiteral("demo clip parse"));
    ok(out.at(0).bold && out.at(0).background == src.background, QStringLiteral("demo clip keeps style"));
}

static void auditDemoLayout(Workbook *wb)
{
    const int sh = sheetIndex(*wb, QStringLiteral("Layout"));
    ok(sh >= 0, QStringLiteral("Layout sheet"));
    if (sh < 0)
        return;
    ok(wb->sheet(sh).freezeRows == 2 && wb->sheet(sh).freezeCols == 1, QStringLiteral("demo freeze"));
    ok(wb->sheet(sh).merges.size() == 1, QStringLiteral("demo merge count"));
    ok(wb->sheet(sh).rowHeights.value(1) == 32, QStringLiteral("demo row height"));
}

static void auditDemoCharts(Workbook *wb)
{
    const int sh = sheetIndex(*wb, QStringLiteral("Charts"));
    ok(sh >= 0, QStringLiteral("Charts sheet"));
    if (sh < 0)
        return;
    ok(wb->sheet(sh).charts.size() >= 1, QStringLiteral("demo chart object"));
    const ChartData data = extractChartData(wb, sh, wb->sheet(sh).charts.first());
    ok(data.valid && data.categories.size() == 5, QStringLiteral("demo chart data points"));
    near(data.series.first().values.last(), 140.0, "demo chart last value");
}

static void auditDemoRoundtrip(Workbook *wb)
{
    QTemporaryDir tmp;
    const QString path = tmp.filePath(QStringLiteral("demo-audit.xlsx"));
    QString err;
    ok(FileIo::save(wb, path, &err), QStringLiteral("demo save %1").arg(err));

    Workbook loaded;
    ok(FileIo::load(&loaded, path, &err), QStringLiteral("demo reload %1").arg(err));
    ok(loaded.sheetCount() == wb->sheetCount(), QStringLiteral("demo sheet count roundtrip"));

    const int f = sheetIndex(loaded, QStringLiteral("Formulas"));
    ok(f >= 0, QStringLiteral("demo Formulas after reload"));
    if (f >= 0)
        near(loaded.displayText(f, 3, 0).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 4.0,
             "demo SUM after reload");

    const int c = sheetIndex(loaded, QStringLiteral("Charts"));
    ok(c >= 0 && loaded.sheet(c).charts.size() >= 1, QStringLiteral("demo charts persisted"));
    if (c >= 0 && !loaded.sheet(c).charts.isEmpty()) {
        const ChartData data = extractChartData(&loaded, c, loaded.sheet(c).charts.first());
        ok(data.valid, QStringLiteral("demo chart data after reload"));
    }

    const int l = sheetIndex(loaded, QStringLiteral("Layout"));
    if (l >= 0)
        ok(loaded.sheet(l).merges.size() == 1, QStringLiteral("demo merge after reload"));
}

static void auditDemoFileOnDisk()
{
    QString path = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../demo/Arbuz-feature-demo.xlsx"));
    if (!QFile::exists(path))
        path = QDir::current().absoluteFilePath(QStringLiteral("demo/Arbuz-feature-demo.xlsx"));
    if (!QFile::exists(path)) {
        std::printf("USER SKIP: demo file not on disk\n");
        return;
    }
    Workbook wb;
    QString err;
    ok(FileIo::load(&wb, path, &err), QStringLiteral("disk demo load %1").arg(err));
    const int f = sheetIndex(wb, QStringLiteral("Formulas"));
    ok(f >= 0, QStringLiteral("disk demo Formulas sheet"));
    if (f >= 0)
        near(wb.displayText(f, 3, 0).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(), 4.0,
             "disk demo SUM");
}

static void auditReplaceSimulation(Workbook *wb)
{
    const int sh = sheetIndex(*wb, QStringLiteral("Data"));
    if (sh < 0)
        return;
    int hitR = -1, hitC = -1;
    for (int r = 1; r <= 6; ++r) {
        for (int c = 0; c <= 3; ++c) {
            if (wb->sheet(sh).cell(r, c).raw.contains(QStringLiteral("REPLACE_ME"))) {
                hitR = r;
                hitC = c;
                break;
            }
        }
        if (hitR >= 0)
            break;
    }
    ok(hitR >= 0, QStringLiteral("demo REPLACE_ME present"));
    if (hitR < 0)
        return;
    CellData d = wb->sheet(sh).cell(hitR, hitC);
    d.raw.replace(QStringLiteral("REPLACE_ME"), QStringLiteral("OK"));
    wb->setCellData(sh, hitR, hitC, d);
    eq(wb->sheet(sh).cell(hitR, hitC).raw, QStringLiteral("OK"), "demo replaced cell");
}

int runUserAudit()
{
    g_fails = 0;
    g_ok = 0;
    I18n::setLang(QStringLiteral("ru"));

    Workbook wb;
    buildDemoWorkbook(&wb);
    ok(wb.sheetCount() == 7, QStringLiteral("demo 7 sheets"));
    {
        const int f = sheetIndex(wb, QStringLiteral("Formulas"));
        eq(wb.displayText(f, 3, 2), QStringLiteral("50"), "immediate post-build VLOOKUP");
    }

    auditDemoFormulas(&wb);
    auditDemoCross(&wb);
    auditReplaceSimulation(&wb);
    auditDemoData(&wb);
    auditDemoFormats(&wb);
    auditDemoLayout(&wb);
    auditDemoCharts(&wb);
    auditDemoRoundtrip(&wb);
    auditDemoFileOnDisk();

    if (g_fails == 0)
        std::printf("user-audit ok (%d checks)\n", g_ok);
    else
        std::fprintf(stderr, "user-audit finished with %d failure(s), %d ok\n", g_fails, g_ok);
    return g_fails == 0 ? 0 : 1;
}
