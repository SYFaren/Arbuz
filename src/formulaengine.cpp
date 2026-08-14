#include "formulaengine.h"
#include "cellref.h"
#include "exceldate.h"

#include "xlfparser.h"

#include <QDate>
#include <QDateTime>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
FormulaEngine::PluginBridge g_pluginBridge;
QVector<FormulaEngine::FormulaInfo> g_pluginCatalog;
}

void FormulaEngine::setPluginBridge(PluginBridge fn, const QVector<FormulaInfo> &extra)
{
    g_pluginBridge = std::move(fn);
    g_pluginCatalog = extra;
}

QVector<FormulaEngine::FormulaInfo> FormulaEngine::pluginCatalog()
{
    return g_pluginCatalog;
}

static FormulaEngine::PluginBridge *pluginBridge()
{
    return g_pluginBridge ? &g_pluginBridge : nullptr;
}

QString FormulaValue::toDisplay() const
{
    switch (kind) {
    case Empty:
        return {};
    case Number: {
        if (std::floor(number) == number && std::fabs(number) < 1e12)
            return QString::number(qint64(number));
        return QString::number(number, 'g', 12);
    }
    case Text:
        return text;
    case Bool:
        return boolean ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    case Error:
        return error;
    }
    return {};
}

double FormulaValue::asNumber(bool *ok) const
{
    if (ok)
        *ok = true;
    switch (kind) {
    case Number:
        return number;
    case Bool:
        return boolean ? 1 : 0;
    case Empty:
        return 0;
    case Text: {
        bool conv = false;
        const double n = text.trimmed().toDouble(&conv);
        if (ok)
            *ok = conv;
        return conv ? n : 0;
    }
    case Error:
        if (ok)
            *ok = false;
        return 0;
    }
    if (ok)
        *ok = false;
    return 0;
}

FormulaEngine::FormulaEngine(CellLookup lookup, int currentSheet, SheetLookup sheets)
    : m_lookup(std::move(lookup))
    , m_sheets(std::move(sheets))
    , m_sheet(currentSheet)
{
}

QVector<FormulaEngine::FormulaInfo> FormulaEngine::catalog()
{
    QVector<FormulaInfo> all = {
        {QStringLiteral("SUM"), QStringLiteral("SUM(range)"), QStringLiteral("math"),
         "Сумма чисел", "Sum of numbers"},
        {QStringLiteral("PRODUCT"), QStringLiteral("PRODUCT(range)"), QStringLiteral("math"),
         "Произведение чисел", "Product of numbers"},
        {QStringLiteral("ABS"), QStringLiteral("ABS(number)"), QStringLiteral("math"),
         "Модуль числа", "Absolute value"},
        {QStringLiteral("SIGN"), QStringLiteral("SIGN(number)"), QStringLiteral("math"),
         "Знак числа (−1, 0, 1)", "Sign of a number (−1, 0, 1)"},
        {QStringLiteral("SQRT"), QStringLiteral("SQRT(number)"), QStringLiteral("math"),
         "Квадратный корень", "Square root"},
        {QStringLiteral("POWER"), QStringLiteral("POWER(number, power)"), QStringLiteral("math"),
         "Возведение в степень", "Exponentiation"},
        {QStringLiteral("MOD"), QStringLiteral("MOD(number, divisor)"), QStringLiteral("math"),
         "Остаток от деления", "Remainder after division"},
        {QStringLiteral("INT"), QStringLiteral("INT(number)"), QStringLiteral("math"),
         "Целая часть (к −∞)", "Integer part (towards −∞)"},
        {QStringLiteral("ROUND"), QStringLiteral("ROUND(number, digits)"), QStringLiteral("math"),
         "Округление", "Round to digits"},
        {QStringLiteral("ROUNDUP"), QStringLiteral("ROUNDUP(number, digits)"), QStringLiteral("math"),
         "Округление вверх", "Round away from zero"},
        {QStringLiteral("ROUNDDOWN"), QStringLiteral("ROUNDDOWN(number, digits)"), QStringLiteral("math"),
         "Округление вниз", "Round towards zero"},
        {QStringLiteral("PI"), QStringLiteral("PI()"), QStringLiteral("math"),
         "Число π", "The constant π"},
        {QStringLiteral("RAND"), QStringLiteral("RAND()"), QStringLiteral("math"),
         "Случайное число 0…1", "Random number 0…1"},
        {QStringLiteral("RANDBETWEEN"), QStringLiteral("RANDBETWEEN(low, high)"), QStringLiteral("math"),
         "Случайное целое в диапазоне", "Random integer in a range"},
        {QStringLiteral("SIN"), QStringLiteral("SIN(number)"), QStringLiteral("math"),
         "Синус", "Sine"},
        {QStringLiteral("COS"), QStringLiteral("COS(number)"), QStringLiteral("math"),
         "Косинус", "Cosine"},
        {QStringLiteral("LN"), QStringLiteral("LN(number)"), QStringLiteral("math"),
         "Натуральный логарифм", "Natural logarithm"},
        {QStringLiteral("LOG"), QStringLiteral("LOG(number, [base])"), QStringLiteral("math"),
         "Логарифм", "Logarithm"},
        {QStringLiteral("EXP"), QStringLiteral("EXP(number)"), QStringLiteral("math"),
         "e в степени", "e raised to a power"},
        {QStringLiteral("SUMPRODUCT"), QStringLiteral("SUMPRODUCT(range, …)"), QStringLiteral("math"),
         "Сумма произведений", "Sum of products"},
        {QStringLiteral("AVERAGE"), QStringLiteral("AVERAGE(range)"), QStringLiteral("stat"),
         "Среднее арифметическое", "Arithmetic mean"},
        {QStringLiteral("MIN"), QStringLiteral("MIN(range)"), QStringLiteral("stat"),
         "Минимум", "Minimum"},
        {QStringLiteral("MAX"), QStringLiteral("MAX(range)"), QStringLiteral("stat"),
         "Максимум", "Maximum"},
        {QStringLiteral("COUNT"), QStringLiteral("COUNT(range)"), QStringLiteral("stat"),
         "Количество чисел", "Count of numbers"},
        {QStringLiteral("COUNTA"), QStringLiteral("COUNTA(range)"), QStringLiteral("stat"),
         "Количество непустых ячеек", "Count of non-empty cells"},
        {QStringLiteral("MEDIAN"), QStringLiteral("MEDIAN(range)"), QStringLiteral("stat"),
         "Медиана", "Median"},
        {QStringLiteral("STDEV"), QStringLiteral("STDEV(range)"), QStringLiteral("stat"),
         "Стандартное отклонение (выборка)", "Sample standard deviation"},
        {QStringLiteral("SUMIF"), QStringLiteral("SUMIF(range, criteria, [sum_range])"), QStringLiteral("stat"),
         "Сумма по условию", "Sum by criterion"},
        {QStringLiteral("COUNTIF"), QStringLiteral("COUNTIF(range, criteria)"), QStringLiteral("stat"),
         "Количество по условию", "Count by criterion"},
        {QStringLiteral("AVERAGEIF"), QStringLiteral("AVERAGEIF(range, criteria, [average_range])"), QStringLiteral("stat"),
         "Среднее по условию", "Average by criterion"},
        {QStringLiteral("SUMIFS"), QStringLiteral("SUMIFS(sum_range, criteria_range, criteria, …)"), QStringLiteral("stat"),
         "Сумма по нескольким условиям", "Sum by multiple criteria"},
        {QStringLiteral("COUNTIFS"), QStringLiteral("COUNTIFS(range, criteria, …)"), QStringLiteral("stat"),
         "Количество по нескольким условиям", "Count by multiple criteria"},
        {QStringLiteral("AVERAGEIFS"), QStringLiteral("AVERAGEIFS(average_range, criteria_range, criteria, …)"), QStringLiteral("stat"),
         "Среднее по нескольким условиям", "Average by multiple criteria"},
        {QStringLiteral("IF"), QStringLiteral("IF(condition, then, else)"), QStringLiteral("logic"),
         "Условие", "Conditional value"},
        {QStringLiteral("AND"), QStringLiteral("AND(a, b, …)"), QStringLiteral("logic"),
         "Истина, если все аргументы истинны", "True if all arguments are true"},
        {QStringLiteral("OR"), QStringLiteral("OR(a, b, …)"), QStringLiteral("logic"),
         "Истина, если любой аргумент истинен", "True if any argument is true"},
        {QStringLiteral("NOT"), QStringLiteral("NOT(value)"), QStringLiteral("logic"),
         "Логическое отрицание", "Logical not"},
        {QStringLiteral("IFERROR"), QStringLiteral("IFERROR(value, fallback)"), QStringLiteral("logic"),
         "Значение или запасной вариант при ошибке", "Value, or fallback on error"},
        {QStringLiteral("TRUE"), QStringLiteral("TRUE()"), QStringLiteral("logic"),
         "Истина", "Boolean true"},
        {QStringLiteral("FALSE"), QStringLiteral("FALSE()"), QStringLiteral("logic"),
         "Ложь", "Boolean false"},
        {QStringLiteral("CONCAT"), QStringLiteral("CONCAT(text, …)"), QStringLiteral("text"),
         "Склеить текст", "Concatenate text"},
        {QStringLiteral("CONCATENATE"), QStringLiteral("CONCATENATE(text, …)"), QStringLiteral("text"),
         "Склеить текст", "Concatenate text"},
        {QStringLiteral("LEFT"), QStringLiteral("LEFT(text, [count])"), QStringLiteral("text"),
         "Символы слева", "Leftmost characters"},
        {QStringLiteral("RIGHT"), QStringLiteral("RIGHT(text, [count])"), QStringLiteral("text"),
         "Символы справа", "Rightmost characters"},
        {QStringLiteral("MID"), QStringLiteral("MID(text, start, count)"), QStringLiteral("text"),
         "Фрагмент текста", "Substring"},
        {QStringLiteral("LEN"), QStringLiteral("LEN(text)"), QStringLiteral("text"),
         "Длина текста", "Text length"},
        {QStringLiteral("TRIM"), QStringLiteral("TRIM(text)"), QStringLiteral("text"),
         "Убрать лишние пробелы", "Trim extra spaces"},
        {QStringLiteral("UPPER"), QStringLiteral("UPPER(text)"), QStringLiteral("text"),
         "Верхний регистр", "Upper case"},
        {QStringLiteral("LOWER"), QStringLiteral("LOWER(text)"), QStringLiteral("text"),
         "Нижний регистр", "Lower case"},
        {QStringLiteral("VALUE"), QStringLiteral("VALUE(text)"), QStringLiteral("text"),
         "Текст в число", "Parse text as number"},
        {QStringLiteral("REPT"), QStringLiteral("REPT(text, times)"), QStringLiteral("text"),
         "Повторить текст", "Repeat text"},
        {QStringLiteral("FIND"), QStringLiteral("FIND(find, within, [start])"), QStringLiteral("text"),
         "Позиция подстроки (с учётом регистра)", "Position of substring (case-sensitive)"},
        {QStringLiteral("SEARCH"), QStringLiteral("SEARCH(find, within, [start])"), QStringLiteral("text"),
         "Позиция подстроки (без учёта регистра)", "Position of substring (case-insensitive)"},
        {QStringLiteral("SUBSTITUTE"), QStringLiteral("SUBSTITUTE(text, old, new, [instance])"), QStringLiteral("text"),
         "Заменить текст", "Replace text"},
        {QStringLiteral("TEXTJOIN"), QStringLiteral("TEXTJOIN(delim, ignore_empty, text, …)"), QStringLiteral("text"),
         "Склеить через разделитель", "Join text with a delimiter"},
        {QStringLiteral("TODAY"), QStringLiteral("TODAY()"), QStringLiteral("date"),
         "Текущая дата", "Current date"},
        {QStringLiteral("NOW"), QStringLiteral("NOW()"), QStringLiteral("date"),
         "Текущие дата и время", "Current date and time"},
        {QStringLiteral("YEAR"), QStringLiteral("YEAR(date)"), QStringLiteral("date"),
         "Год", "Year"},
        {QStringLiteral("MONTH"), QStringLiteral("MONTH(date)"), QStringLiteral("date"),
         "Месяц", "Month"},
        {QStringLiteral("DAY"), QStringLiteral("DAY(date)"), QStringLiteral("date"),
         "День", "Day"},
        {QStringLiteral("DATE"), QStringLiteral("DATE(year, month, day)"), QStringLiteral("date"),
         "Собрать дату", "Build a date"},
        {QStringLiteral("WEEKDAY"), QStringLiteral("WEEKDAY(date, [type])"), QStringLiteral("date"),
         "День недели", "Day of week"},
        {QStringLiteral("EOMONTH"), QStringLiteral("EOMONTH(date, months)"), QStringLiteral("date"),
         "Конец месяца", "End of month"},
        {QStringLiteral("VLOOKUP"), QStringLiteral("VLOOKUP(value, table, col, [exact])"), QStringLiteral("lookup"),
         "Вертикальный поиск в таблице", "Vertical lookup in a table"},
        {QStringLiteral("HLOOKUP"), QStringLiteral("HLOOKUP(value, table, row, [exact])"), QStringLiteral("lookup"),
         "Горизонтальный поиск в таблице", "Horizontal lookup in a table"},
        {QStringLiteral("INDEX"), QStringLiteral("INDEX(range, row, [column])"), QStringLiteral("lookup"),
         "Значение по номеру строки/столбца", "Value by row/column index"},
        {QStringLiteral("MATCH"), QStringLiteral("MATCH(value, range, [type])"), QStringLiteral("lookup"),
         "Позиция значения в диапазоне", "Position of a value in a range"},
        {QStringLiteral("CHOOSE"), QStringLiteral("CHOOSE(index, value1, …)"), QStringLiteral("lookup"),
         "Значение по номеру", "Value by 1-based index"},
        {QStringLiteral("ISBLANK"), QStringLiteral("ISBLANK(value)"), QStringLiteral("info"),
         "Ячейка пуста", "Cell is empty"},
        {QStringLiteral("ISNUMBER"), QStringLiteral("ISNUMBER(value)"), QStringLiteral("info"),
         "Значение — число", "Value is a number"},
        {QStringLiteral("ISTEXT"), QStringLiteral("ISTEXT(value)"), QStringLiteral("info"),
         "Значение — текст", "Value is text"},
        {QStringLiteral("ISERROR"), QStringLiteral("ISERROR(value)"), QStringLiteral("info"),
         "Значение — ошибка", "Value is an error"},
        {QStringLiteral("N"), QStringLiteral("N(value)"), QStringLiteral("info"),
         "Привести к числу", "Coerce to number"},
        {QStringLiteral("NA"), QStringLiteral("NA()"), QStringLiteral("info"),
         "Ошибка #N/A", "The #N/A error"},
    };
    all += pluginCatalog();
    return all;
}

QStringList FormulaEngine::categories()
{
    return {QStringLiteral("math"), QStringLiteral("stat"), QStringLiteral("logic"),
            QStringLiteral("text"), QStringLiteral("date"), QStringLiteral("lookup"),
            QStringLiteral("info"), QStringLiteral("plugin")};
}

QStringList FormulaEngine::dependencyRefs(const QString &formula)
{
    QStringList refs;
    QString f = formula.trimmed();
    if (!f.startsWith(QLatin1Char('=')))
        return refs;
    try {
        const std::string src = f.toStdString();
        const auto tokens = xlfparser::tokenize(src);
        for (const auto &tok : tokens) {
            if (tok.type() != xlfparser::Token::Type::Operand)
                continue;
            if (tok.subtype() != xlfparser::Token::Subtype::Range)
                continue;
            refs.append(QString::fromStdString(tok.value(src)));
        }
    } catch (const std::exception &) {
        // invalid formula — no deps
    }
    return refs;
}

FormulaValue FormulaEngine::evaluate(const QString &formula)
{
    QString f = formula.trimmed();
    if (!f.startsWith(QLatin1Char('=')))
        return FormulaValue::fromText(f);

    try {
        xlfparser::tokenize(f.toStdString());
    } catch (const std::exception &) {
        // xlfparser is a syntax hint; our parser accepts Sheet!A1 and quoted names.
    }

    m_src = f.mid(1);
    m_pos = 0;
    FormulaValue v = parseExpr();
    skipWs();
    if (m_pos < m_src.size() && !v.isError())
        return FormulaValue::fromError(QStringLiteral("#VALUE!"));
    return v;
}

void FormulaEngine::skipWs()
{
    while (m_pos < m_src.size() && m_src.at(m_pos).isSpace())
        ++m_pos;
}

bool FormulaEngine::match(QChar c)
{
    skipWs();
    if (m_pos < m_src.size() && m_src.at(m_pos) == c) {
        ++m_pos;
        return true;
    }
    return false;
}

bool FormulaEngine::matchSep()
{
    return match(QLatin1Char(',')) || match(QLatin1Char(';'));
}

FormulaValue FormulaEngine::parseExpr()
{
    return parseCompare();
}

static bool isError(const FormulaValue &a, const FormulaValue &b, FormulaValue *out)
{
    if (a.isError()) {
        *out = a;
        return true;
    }
    if (b.isError()) {
        *out = b;
        return true;
    }
    return false;
}

FormulaValue FormulaEngine::parseCompare()
{
    FormulaValue left = parseConcat();
    skipWs();
    if (m_pos >= m_src.size())
        return left;

    QString op;
    if (m_pos + 1 < m_src.size()) {
        const QString two = m_src.mid(m_pos, 2);
        if (two == QLatin1String("<>") || two == QLatin1String("<=") || two == QLatin1String(">=")) {
            op = two;
            m_pos += 2;
        }
    }
    if (op.isEmpty()) {
        const QChar c = m_src.at(m_pos);
        if (c == QLatin1Char('=') || c == QLatin1Char('<') || c == QLatin1Char('>')) {
            op = QString(c);
            ++m_pos;
        }
    }
    if (op.isEmpty())
        return left;

    FormulaValue right = parseConcat();
    FormulaValue err;
    if (isError(left, right, &err))
        return err;

    bool okL = false, okR = false;
    const double a = left.asNumber(&okL);
    const double b = right.asNumber(&okR);
    bool cmp = false;
    if (okL && okR) {
        if (op == QLatin1String("="))
            cmp = qFuzzyCompare(a + 1, b + 1);
        else if (op == QLatin1String("<>"))
            cmp = !qFuzzyCompare(a + 1, b + 1);
        else if (op == QLatin1String("<"))
            cmp = a < b;
        else if (op == QLatin1String(">"))
            cmp = a > b;
        else if (op == QLatin1String("<="))
            cmp = a <= b;
        else if (op == QLatin1String(">="))
            cmp = a >= b;
    } else {
        const QString sa = left.toDisplay();
        const QString sb = right.toDisplay();
        if (op == QLatin1String("="))
            cmp = sa.compare(sb, Qt::CaseInsensitive) == 0;
        else if (op == QLatin1String("<>"))
            cmp = sa.compare(sb, Qt::CaseInsensitive) != 0;
        else if (op == QLatin1String("<"))
            cmp = QString::compare(sa, sb, Qt::CaseInsensitive) < 0;
        else if (op == QLatin1String(">"))
            cmp = QString::compare(sa, sb, Qt::CaseInsensitive) > 0;
        else if (op == QLatin1String("<="))
            cmp = QString::compare(sa, sb, Qt::CaseInsensitive) <= 0;
        else if (op == QLatin1String(">="))
            cmp = QString::compare(sa, sb, Qt::CaseInsensitive) >= 0;
    }
    return FormulaValue::fromBool(cmp);
}

FormulaValue FormulaEngine::parseConcat()
{
    FormulaValue left = parseAdd();
    while (match(QLatin1Char('&'))) {
        FormulaValue right = parseAdd();
        if (left.isError())
            return left;
        if (right.isError())
            return right;
        left = FormulaValue::fromText(left.toDisplay() + right.toDisplay());
    }
    return left;
}

FormulaValue FormulaEngine::parseAdd()
{
    FormulaValue left = parseMul();
    for (;;) {
        if (match(QLatin1Char('+'))) {
            FormulaValue right = parseMul();
            FormulaValue err;
            if (isError(left, right, &err))
                return err;
            left = FormulaValue::fromNumber(left.asNumber() + right.asNumber());
        } else if (match(QLatin1Char('-'))) {
            FormulaValue right = parseMul();
            FormulaValue err;
            if (isError(left, right, &err))
                return err;
            left = FormulaValue::fromNumber(left.asNumber() - right.asNumber());
        } else {
            break;
        }
    }
    return left;
}

FormulaValue FormulaEngine::parseMul()
{
    FormulaValue left = parsePower();
    for (;;) {
        if (match(QLatin1Char('*'))) {
            FormulaValue right = parsePower();
            FormulaValue err;
            if (isError(left, right, &err))
                return err;
            left = FormulaValue::fromNumber(left.asNumber() * right.asNumber());
        } else if (match(QLatin1Char('/'))) {
            FormulaValue right = parsePower();
            FormulaValue err;
            if (isError(left, right, &err))
                return err;
            const double d = right.asNumber();
            if (qFuzzyIsNull(d))
                return FormulaValue::fromError(QStringLiteral("#DIV/0!"));
            left = FormulaValue::fromNumber(left.asNumber() / d);
        } else {
            break;
        }
    }
    return left;
}

FormulaValue FormulaEngine::parsePower()
{
    FormulaValue left = parseUnary();
    skipWs();
    if (!match(QLatin1Char('^')))
        return left;
    FormulaValue right = parsePower();
    FormulaValue err;
    if (isError(left, right, &err))
        return err;
    return FormulaValue::fromNumber(std::pow(left.asNumber(), right.asNumber()));
}

FormulaValue FormulaEngine::parseUnary()
{
    if (match(QLatin1Char('+')))
        return parseUnary();
    if (match(QLatin1Char('-'))) {
        FormulaValue v = parseUnary();
        if (v.isError())
            return v;
        v.number = -v.asNumber();
        v.kind = FormulaValue::Number;
        return v;
    }
    return parsePrimary();
}

FormulaValue FormulaEngine::parsePrimary()
{
    skipWs();
    if (m_pos >= m_src.size())
        return FormulaValue::fromError(QStringLiteral("#VALUE!"));

    if (match(QLatin1Char('('))) {
        FormulaValue v = parseExpr();
        if (!match(QLatin1Char(')')))
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return v;
    }

    if (m_src.at(m_pos) == QLatin1Char('"')) {
        ++m_pos;
        QString t;
        while (m_pos < m_src.size()) {
            if (m_src.at(m_pos) == QLatin1Char('"')) {
                ++m_pos;
                if (m_pos < m_src.size() && m_src.at(m_pos) == QLatin1Char('"')) {
                    t.append(QLatin1Char('"'));
                    ++m_pos;
                    continue;
                }
                break;
            }
            t.append(m_src.at(m_pos));
            ++m_pos;
        }
        return FormulaValue::fromText(t);
    }

    if (m_src.at(m_pos).isDigit() || m_src.at(m_pos) == QLatin1Char('.')) {
        int start = m_pos;
        while (m_pos < m_src.size() && (m_src.at(m_pos).isDigit() || m_src.at(m_pos) == QLatin1Char('.')
                                        || m_src.at(m_pos).toUpper() == QLatin1Char('E')
                                        || ((m_src.at(m_pos) == QLatin1Char('+') || m_src.at(m_pos) == QLatin1Char('-'))
                                            && m_pos > start && m_src.at(m_pos - 1).toUpper() == QLatin1Char('E'))))
            ++m_pos;
        bool ok = false;
        const double n = m_src.mid(start, m_pos - start).toDouble(&ok);
        if (!ok)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return FormulaValue::fromNumber(n);
    }

    if (m_src.at(m_pos) == QLatin1Char('\'') || m_src.at(m_pos) == QLatin1Char('$') || m_src.at(m_pos).isLetter()) {
        const int start = m_pos;
        if (m_src.at(m_pos) == QLatin1Char('\'')) {
            ++m_pos;
            while (m_pos < m_src.size()) {
                if (m_src.at(m_pos) == QLatin1Char('\'')) {
                    ++m_pos;
                    if (m_pos < m_src.size() && m_src.at(m_pos) == QLatin1Char('\'')) {
                        ++m_pos;
                        continue;
                    }
                    break;
                }
                ++m_pos;
            }
            skipWs();
            if (!match(QLatin1Char('!'))) {
                m_pos = start;
                return FormulaValue::fromError(QStringLiteral("#NAME?"));
            }
        }
        while (m_pos < m_src.size()) {
            const QChar c = m_src.at(m_pos);
            if (c.isLetterOrNumber() || c == QLatin1Char('$') || c == QLatin1Char('_') || c == QLatin1Char('.'))
                ++m_pos;
            else
                break;
        }
        skipWs();
        if (m_pos < m_src.size() && m_src.at(m_pos) == QLatin1Char('!')) {
            ++m_pos;
            skipWs();
            while (m_pos < m_src.size()) {
                const QChar c = m_src.at(m_pos);
                if (c.isLetterOrNumber() || c == QLatin1Char('$'))
                    ++m_pos;
                else
                    break;
            }
        }

        QString ident = m_src.mid(start, m_pos - start);

        skipWs();
        if (m_pos < m_src.size() && m_src.at(m_pos) == QLatin1Char(':')) {
            ++m_pos;
            skipWs();
            int start2 = m_pos;
            while (m_pos < m_src.size()) {
                const QChar c = m_src.at(m_pos);
                if (c.isLetterOrNumber() || c == QLatin1Char('$'))
                    ++m_pos;
                else
                    break;
            }
            ident += QLatin1Char(':') + m_src.mid(start2, m_pos - start2);
            CellRef::Addr a, b;
            if (!CellRef::parseRangeRef(ident, &a, &b))
                return FormulaValue::fromError(QStringLiteral("#REF!"));
            const int sh = a.sheet.isEmpty() ? m_sheet : resolveSheet(a.sheet);
            if (sh < 0)
                return FormulaValue::fromError(QStringLiteral("#REF!"));
            QVector<FormulaValue> vals;
            collectRange(sh, qMin(a.row, b.row), qMin(a.col, b.col), qMax(a.row, b.row), qMax(a.col, b.col), &vals);
            if (vals.size() == 1)
                return vals.first();
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        }

        skipWs();
        if (match(QLatin1Char('(')))
            return parseFunction(ident.toUpper());

        CellRef::Addr addr;
        if (CellRef::parseAddr(ident, &addr) && addr.valid) {
            const int sh = addr.sheet.isEmpty() ? m_sheet : resolveSheet(addr.sheet);
            if (sh < 0 || addr.row < 0 || addr.col < 0)
                return FormulaValue::fromError(QStringLiteral("#REF!"));
            return m_lookup(sh, addr.row, addr.col);
        }

        const QString up = ident.toUpper();
        if (up == QLatin1String("TRUE"))
            return FormulaValue::fromBool(true);
        if (up == QLatin1String("FALSE"))
            return FormulaValue::fromBool(false);
        return FormulaValue::fromError(QStringLiteral("#NAME?"));
    }

    return FormulaValue::fromError(QStringLiteral("#VALUE!"));
}

QVector<FormulaArg> FormulaEngine::parseArgs()
{
    QVector<FormulaArg> args;
    skipWs();
    if (match(QLatin1Char(')')))
        return args;
    for (;;) {
        skipWs();
        const int save = m_pos;
        int end = m_pos;
        while (end < m_src.size() && m_src.at(end) != QLatin1Char(',') && m_src.at(end) != QLatin1Char(';')
               && m_src.at(end) != QLatin1Char(')'))
            ++end;
        const QString slice = m_src.mid(m_pos, end - m_pos).trimmed();
        FormulaArg arg;
        CellRef::Addr ra, rb;
        if (CellRef::parseRangeRef(slice, &ra, &rb)
            && (slice.contains(QLatin1Char(':')) || CellRef::parseAddr(slice, &ra))) {
            arg.isRange = true;
            arg.r1 = qMin(ra.row, rb.row);
            arg.c1 = qMin(ra.col, rb.col);
            arg.r2 = qMax(ra.row, rb.row);
            arg.c2 = qMax(ra.col, rb.col);
            arg.sheet = ra.sheet.isEmpty() ? -1 : resolveSheet(ra.sheet);
            if (!ra.sheet.isEmpty() && arg.sheet < 0)
                arg.value = FormulaValue::fromError(QStringLiteral("#REF!"));
            m_pos = end;
        } else {
            m_pos = save;
            arg.value = parseExpr();
        }
        args.append(arg);
        if (matchSep())
            continue;
        if (match(QLatin1Char(')')))
            break;
        FormulaArg err;
        err.value = FormulaValue::fromError(QStringLiteral("#VALUE!"));
        args.append(err);
        break;
    }
    return args;
}

int FormulaEngine::resolveSheet(const QString &name) const
{
    if (!m_sheets)
        return -1;
    return m_sheets(name);
}

int FormulaEngine::argSheet(const FormulaArg &a) const
{
    return a.sheet >= 0 ? a.sheet : m_sheet;
}

void FormulaEngine::collectRange(int sheet, int r1, int c1, int r2, int c2, QVector<FormulaValue> *out) const
{
    const int sh = sheet >= 0 ? sheet : m_sheet;
    for (int r = r1; r <= r2; ++r) {
        for (int c = c1; c <= c2; ++c)
            out->append(m_lookup(sh, r, c));
    }
}

QVector<FormulaValue> FormulaEngine::flatten(const QVector<FormulaArg> &args) const
{
    QVector<FormulaValue> out;
    for (const FormulaArg &a : args) {
        if (a.isRange)
            collectRange(argSheet(a), a.r1, a.c1, a.r2, a.c2, &out);
        else
            out.append(a.value);
    }
    return out;
}

FormulaValue FormulaEngine::parseFunction(const QString &name)
{
    const QVector<FormulaArg> args = parseArgs();
    const bool skipErr = name == QLatin1String("IF") || name == QLatin1String("IFERROR")
        || name == QLatin1String("ISERROR") || name == QLatin1String("ISBLANK")
        || name == QLatin1String("ISNUMBER") || name == QLatin1String("ISTEXT");
    if (!skipErr) {
        for (const FormulaArg &a : args) {
            if (!a.isRange && a.value.isError())
                return a.value;
            if (a.isRange) {
                QVector<FormulaValue> cells;
                collectRange(argSheet(a), a.r1, a.c1, a.r2, a.c2, &cells);
                for (const FormulaValue &c : cells) {
                    if (c.isError() && name != QLatin1String("SUMIF") && name != QLatin1String("COUNTIF")
                        && name != QLatin1String("AVERAGEIF") && name != QLatin1String("COUNTA")
                        && name != QLatin1String("SUMIFS") && name != QLatin1String("COUNTIFS")
                        && name != QLatin1String("AVERAGEIFS"))
                        return c;
                }
            }
        }
    }

    auto val = [&](int i) -> FormulaValue {
        if (i < 0 || i >= args.size())
            return FormulaValue();
        if (args.at(i).isRange) {
            QVector<FormulaValue> cells;
            collectRange(argSheet(args.at(i)), args.at(i).r1, args.at(i).c1, args.at(i).r2, args.at(i).c2, &cells);
            return cells.isEmpty() ? FormulaValue() : cells.first();
        }
        return args.at(i).value;
    };
    auto nums = [&]() {
        QVector<double> out;
        for (const FormulaValue &a : flatten(args)) {
            if (a.kind == FormulaValue::Empty || a.kind == FormulaValue::Text || a.isError())
                continue;
            bool ok = false;
            const double n = a.asNumber(&ok);
            if (ok)
                out.append(n);
        }
        return out;
    };
    auto truthy = [](const FormulaValue &v) {
        if (v.isError())
            return false;
        if (v.kind == FormulaValue::Bool)
            return v.boolean;
        if (v.kind == FormulaValue::Number)
            return !qFuzzyIsNull(v.number);
        return !v.toDisplay().isEmpty();
    };
    auto roundTo = [](double n, int digits, int mode) {
        const double f = std::pow(10.0, digits);
        double x = n * f;
        if (mode == 0)
            x = std::nearbyint(x);
        else if (mode > 0)
            x = n >= 0 ? std::ceil(x - 1e-12) : std::floor(x + 1e-12);
        else
            x = n >= 0 ? std::floor(x + 1e-12) : std::ceil(x - 1e-12);
        return x / f;
    };
    auto matchCrit = [](const FormulaValue &cell, const FormulaValue &crit) {
        QString s = crit.toDisplay();
        auto cmpOp = [&](const QString &op, const QString &rest) {
            bool okC = false, okV = false;
            const double c = rest.trimmed().toDouble(&okC);
            const double v = cell.asNumber(&okV);
            if (okC && okV) {
                if (op == QLatin1String(">"))
                    return v > c;
                if (op == QLatin1String("<"))
                    return v < c;
                if (op == QLatin1String(">="))
                    return v >= c;
                if (op == QLatin1String("<="))
                    return v <= c;
                if (op == QLatin1String("<>"))
                    return !qFuzzyCompare(v + 1, c + 1);
                return qFuzzyCompare(v + 1, c + 1);
            }
            const int cmp = QString::compare(cell.toDisplay(), rest, Qt::CaseInsensitive);
            if (op == QLatin1String(">"))
                return cmp > 0;
            if (op == QLatin1String("<"))
                return cmp < 0;
            if (op == QLatin1String(">="))
                return cmp >= 0;
            if (op == QLatin1String("<="))
                return cmp <= 0;
            if (op == QLatin1String("<>"))
                return cmp != 0;
            return cmp == 0;
        };
        if (s.startsWith(QLatin1String(">=")))
            return cmpOp(QStringLiteral(">="), s.mid(2));
        if (s.startsWith(QLatin1String("<=")))
            return cmpOp(QStringLiteral("<="), s.mid(2));
        if (s.startsWith(QLatin1String("<>")))
            return cmpOp(QStringLiteral("<>"), s.mid(2));
        if (s.startsWith(QLatin1Char('>')))
            return cmpOp(QStringLiteral(">"), s.mid(1));
        if (s.startsWith(QLatin1Char('<')))
            return cmpOp(QStringLiteral("<"), s.mid(1));
        if (s.startsWith(QLatin1Char('=')))
            s = s.mid(1);
        if (s.contains(QLatin1Char('*')) || s.contains(QLatin1Char('?'))) {
            QRegularExpression re(QRegularExpression::wildcardToRegularExpression(s));
            re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
            return re.match(cell.toDisplay()).hasMatch();
        }
        if (crit.kind == FormulaValue::Number || crit.kind == FormulaValue::Bool) {
            bool ok = false;
            const double n = cell.asNumber(&ok);
            return ok && qFuzzyCompare(n + 1, crit.asNumber() + 1);
        }
        bool okC = false;
        const double c = s.toDouble(&okC);
        if (okC) {
            bool okV = false;
            const double v = cell.asNumber(&okV);
            return okV && qFuzzyCompare(v + 1, c + 1);
        }
        return cell.toDisplay().compare(s, Qt::CaseInsensitive) == 0;
    };
    auto parseDate = [](const FormulaValue &v) {
        if (v.kind == FormulaValue::Number)
            return ExcelDate::toDate(v.number);
        QDate d = QDate::fromString(v.toDisplay().left(10), Qt::ISODate);
        if (!d.isValid())
            d = QDate::fromString(v.toDisplay(), QStringLiteral("dd.MM.yyyy"));
        if (!d.isValid()) {
            bool ok = false;
            const double n = v.toDisplay().toDouble(&ok);
            if (ok)
                d = ExcelDate::toDate(n);
        }
        return d;
    };
    auto condAgg = [&](bool doSum, bool doCount, bool doAvg) {
        if (args.size() < 2 || !args.at(0).isRange)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const FormulaArg &rng = args.at(0);
        const FormulaValue crit = val(1);
        FormulaArg sumR = rng;
        if (args.size() >= 3 && args.at(2).isRange)
            sumR = args.at(2);
        double s = 0;
        int n = 0;
        const int rows = rng.r2 - rng.r1 + 1;
        const int cols = rng.c2 - rng.c1 + 1;
        const int shR = argSheet(rng);
        const int shS = argSheet(sumR);
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                const FormulaValue cell = m_lookup(shR, rng.r1 + i, rng.c1 + j);
                if (!matchCrit(cell, crit))
                    continue;
                const FormulaValue add = m_lookup(shS, sumR.r1 + i, sumR.c1 + j);
                bool ok = false;
                const double x = add.asNumber(&ok);
                if (doCount)
                    ++n;
                if (ok) {
                    s += x;
                    if (!doCount)
                        ++n;
                }
            }
        }
        if (doAvg) {
            if (n == 0)
                return FormulaValue::fromError(QStringLiteral("#DIV/0!"));
            return FormulaValue::fromNumber(s / n);
        }
        if (doCount)
            return FormulaValue::fromNumber(n);
        return FormulaValue::fromNumber(s);
    };

    if (name == QLatin1String("SUM")) {
        double s = 0;
        for (double n : nums())
            s += n;
        return FormulaValue::fromNumber(s);
    }
    if (name == QLatin1String("PRODUCT")) {
        const auto v = nums();
        if (v.isEmpty())
            return FormulaValue::fromNumber(0);
        double p = 1;
        for (double n : v)
            p *= n;
        return FormulaValue::fromNumber(p);
    }
    if (name == QLatin1String("ABS"))
        return FormulaValue::fromNumber(std::fabs(val(0).asNumber()));
    if (name == QLatin1String("SIGN")) {
        const double n = val(0).asNumber();
        return FormulaValue::fromNumber(n > 0 ? 1 : (n < 0 ? -1 : 0));
    }
    if (name == QLatin1String("SQRT")) {
        const double n = val(0).asNumber();
        if (n < 0)
            return FormulaValue::fromError(QStringLiteral("#NUM!"));
        return FormulaValue::fromNumber(std::sqrt(n));
    }
    if (name == QLatin1String("SIN"))
        return FormulaValue::fromNumber(std::sin(val(0).asNumber()));
    if (name == QLatin1String("COS"))
        return FormulaValue::fromNumber(std::cos(val(0).asNumber()));
    if (name == QLatin1String("LN")) {
        const double n = val(0).asNumber();
        if (n <= 0)
            return FormulaValue::fromError(QStringLiteral("#NUM!"));
        return FormulaValue::fromNumber(std::log(n));
    }
    if (name == QLatin1String("LOG")) {
        const double n = val(0).asNumber();
        const double base = args.size() > 1 ? val(1).asNumber() : 10.0;
        if (n <= 0 || base <= 0 || qFuzzyCompare(base + 1, 1))
            return FormulaValue::fromError(QStringLiteral("#NUM!"));
        return FormulaValue::fromNumber(std::log(n) / std::log(base));
    }
    if (name == QLatin1String("EXP"))
        return FormulaValue::fromNumber(std::exp(val(0).asNumber()));
    if (name == QLatin1String("SUMPRODUCT")) {
        if (args.isEmpty() || !args.at(0).isRange)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const int rows = args.at(0).r2 - args.at(0).r1 + 1;
        const int cols = args.at(0).c2 - args.at(0).c1 + 1;
        double sum = 0;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                double p = 1;
                for (const FormulaArg &a : args) {
                    if (!a.isRange)
                        return FormulaValue::fromError(QStringLiteral("#VALUE!"));
                    p *= m_lookup(argSheet(a), a.r1 + i, a.c1 + j).asNumber();
                }
                sum += p;
            }
        }
        return FormulaValue::fromNumber(sum);
    }
    if (name == QLatin1String("POWER")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return FormulaValue::fromNumber(std::pow(val(0).asNumber(), val(1).asNumber()));
    }
    if (name == QLatin1String("MOD")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const double b = val(1).asNumber();
        if (qFuzzyIsNull(b))
            return FormulaValue::fromError(QStringLiteral("#DIV/0!"));
        const double a = val(0).asNumber();
        return FormulaValue::fromNumber(a - b * std::floor(a / b));
    }
    if (name == QLatin1String("INT"))
        return FormulaValue::fromNumber(std::floor(val(0).asNumber()));
    if (name == QLatin1String("ROUND"))
        return FormulaValue::fromNumber(roundTo(val(0).asNumber(), args.size() > 1 ? int(val(1).asNumber()) : 0, 0));
    if (name == QLatin1String("ROUNDUP"))
        return FormulaValue::fromNumber(roundTo(val(0).asNumber(), args.size() > 1 ? int(val(1).asNumber()) : 0, 1));
    if (name == QLatin1String("ROUNDDOWN"))
        return FormulaValue::fromNumber(roundTo(val(0).asNumber(), args.size() > 1 ? int(val(1).asNumber()) : 0, -1));
    if (name == QLatin1String("PI"))
        return FormulaValue::fromNumber(3.141592653589793);
    if (name == QLatin1String("RAND"))
        return FormulaValue::fromNumber(QRandomGenerator::global()->generateDouble());
    if (name == QLatin1String("RANDBETWEEN")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        int lo = int(std::floor(val(0).asNumber()));
        int hi = int(std::floor(val(1).asNumber()));
        if (lo > hi)
            std::swap(lo, hi);
        return FormulaValue::fromNumber(QRandomGenerator::global()->bounded(lo, hi + 1));
    }
    if (name == QLatin1String("AVERAGE")) {
        const auto v = nums();
        if (v.isEmpty())
            return FormulaValue::fromError(QStringLiteral("#DIV/0!"));
        double s = 0;
        for (double n : v)
            s += n;
        return FormulaValue::fromNumber(s / v.size());
    }
    if (name == QLatin1String("MIN") || name == QLatin1String("MAX")) {
        const auto v = nums();
        if (v.isEmpty())
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        double m = v.first();
        for (double n : v)
            m = (name == QLatin1String("MIN")) ? std::min(m, n) : std::max(m, n);
        return FormulaValue::fromNumber(m);
    }
    if (name == QLatin1String("COUNT"))
        return FormulaValue::fromNumber(nums().size());
    if (name == QLatin1String("COUNTA")) {
        int n = 0;
        for (const FormulaValue &a : flatten(args)) {
            if (!a.isEmpty())
                ++n;
        }
        return FormulaValue::fromNumber(n);
    }
    if (name == QLatin1String("MEDIAN")) {
        auto v = nums();
        if (v.isEmpty())
            return FormulaValue::fromError(QStringLiteral("#NUM!"));
        std::sort(v.begin(), v.end());
        const int n = v.size();
        if (n % 2)
            return FormulaValue::fromNumber(v.at(n / 2));
        return FormulaValue::fromNumber((v.at(n / 2 - 1) + v.at(n / 2)) / 2.0);
    }
    if (name == QLatin1String("STDEV")) {
        const auto v = nums();
        if (v.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#DIV/0!"));
        double mean = 0;
        for (double n : v)
            mean += n;
        mean /= v.size();
        double acc = 0;
        for (double n : v)
            acc += (n - mean) * (n - mean);
        return FormulaValue::fromNumber(std::sqrt(acc / (v.size() - 1)));
    }
    if (name == QLatin1String("SUMIF"))
        return condAgg(true, false, false);
    if (name == QLatin1String("COUNTIF"))
        return condAgg(false, true, false);
    if (name == QLatin1String("AVERAGEIF"))
        return condAgg(false, false, true);
    if (name == QLatin1String("SUMIFS") || name == QLatin1String("COUNTIFS") || name == QLatin1String("AVERAGEIFS")) {
        if (args.size() < 3)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const bool isCount = name == QLatin1String("COUNTIFS");
        int a0 = 0;
        FormulaArg sumR;
        if (!isCount) {
            if (!args.at(0).isRange)
                return FormulaValue::fromError(QStringLiteral("#VALUE!"));
            sumR = args.at(0);
            a0 = 1;
        }
        if ((args.size() - a0) % 2 != 0)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        if (isCount) {
            if (!args.at(0).isRange)
                return FormulaValue::fromError(QStringLiteral("#VALUE!"));
            sumR = args.at(0);
        }
        const int rows = args.at(a0).r2 - args.at(a0).r1 + 1;
        const int cols = args.at(a0).c2 - args.at(a0).c1 + 1;
        double s = 0;
        int n = 0;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                bool ok = true;
                for (int a = a0; a < args.size(); a += 2) {
                    if (!args.at(a).isRange) {
                        ok = false;
                        break;
                    }
                    const FormulaValue cell = m_lookup(argSheet(args.at(a)), args.at(a).r1 + i, args.at(a).c1 + j);
                    if (!matchCrit(cell, val(a + 1))) {
                        ok = false;
                        break;
                    }
                }
                if (!ok)
                    continue;
                ++n;
                if (!isCount) {
                    bool numOk = false;
                    const double x = m_lookup(argSheet(sumR), sumR.r1 + i, sumR.c1 + j).asNumber(&numOk);
                    if (numOk)
                        s += x;
                }
            }
        }
        if (name == QLatin1String("COUNTIFS"))
            return FormulaValue::fromNumber(n);
        if (name == QLatin1String("AVERAGEIFS")) {
            if (n == 0)
                return FormulaValue::fromError(QStringLiteral("#DIV/0!"));
            return FormulaValue::fromNumber(s / n);
        }
        return FormulaValue::fromNumber(s);
    }
    if (name == QLatin1String("IF")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const FormulaValue cond = val(0);
        if (cond.isError())
            return cond;
        if (truthy(cond))
            return val(1);
        if (args.size() >= 3)
            return val(2);
        return FormulaValue::fromBool(false);
    }
    if (name == QLatin1String("AND")) {
        if (args.isEmpty())
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        for (const FormulaValue &a : flatten(args)) {
            if (a.isError())
                return a;
            if (!truthy(a))
                return FormulaValue::fromBool(false);
        }
        return FormulaValue::fromBool(true);
    }
    if (name == QLatin1String("OR")) {
        if (args.isEmpty())
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        for (const FormulaValue &a : flatten(args)) {
            if (a.isError())
                return a;
            if (truthy(a))
                return FormulaValue::fromBool(true);
        }
        return FormulaValue::fromBool(false);
    }
    if (name == QLatin1String("NOT"))
        return FormulaValue::fromBool(!truthy(val(0)));
    if (name == QLatin1String("IFERROR")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const FormulaValue v = val(0);
        return v.isError() ? val(1) : v;
    }
    if (name == QLatin1String("TRUE"))
        return FormulaValue::fromBool(true);
    if (name == QLatin1String("FALSE"))
        return FormulaValue::fromBool(false);
    if (name == QLatin1String("CONCAT") || name == QLatin1String("CONCATENATE")) {
        QString t;
        for (const FormulaValue &a : flatten(args))
            t += a.toDisplay();
        return FormulaValue::fromText(t);
    }
    if (name == QLatin1String("LEFT")) {
        const QString t = val(0).toDisplay();
        int n = args.size() > 1 ? int(val(1).asNumber()) : 1;
        n = qBound(0, n, t.size());
        return FormulaValue::fromText(t.left(n));
    }
    if (name == QLatin1String("RIGHT")) {
        const QString t = val(0).toDisplay();
        int n = args.size() > 1 ? int(val(1).asNumber()) : 1;
        n = qBound(0, n, t.size());
        return FormulaValue::fromText(t.right(n));
    }
    if (name == QLatin1String("MID")) {
        if (args.size() < 3)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const QString t = val(0).toDisplay();
        const int start = int(val(1).asNumber()) - 1;
        const int n = int(val(2).asNumber());
        if (start < 0 || n < 0)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return FormulaValue::fromText(t.mid(start, n));
    }
    if (name == QLatin1String("LEN"))
        return FormulaValue::fromNumber(val(0).toDisplay().size());
    if (name == QLatin1String("TRIM"))
        return FormulaValue::fromText(val(0).toDisplay().simplified());
    if (name == QLatin1String("UPPER"))
        return FormulaValue::fromText(val(0).toDisplay().toUpper());
    if (name == QLatin1String("LOWER"))
        return FormulaValue::fromText(val(0).toDisplay().toLower());
    if (name == QLatin1String("VALUE")) {
        bool ok = false;
        const double n = val(0).toDisplay().trimmed().toDouble(&ok);
        if (!ok)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return FormulaValue::fromNumber(n);
    }
    if (name == QLatin1String("REPT")) {
        const QString t = val(0).toDisplay();
        const int n = qMax(0, int(val(1).asNumber()));
        return FormulaValue::fromText(t.repeated(n));
    }
    if (name == QLatin1String("FIND")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const QString find = val(0).toDisplay();
        const QString within = val(1).toDisplay();
        int start = args.size() > 2 ? int(val(2).asNumber()) - 1 : 0;
        if (start < 0)
            start = 0;
        const int pos = within.indexOf(find, start, Qt::CaseSensitive);
        if (pos < 0)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return FormulaValue::fromNumber(pos + 1);
    }
    if (name == QLatin1String("SEARCH")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const QString find = val(0).toDisplay();
        const QString within = val(1).toDisplay();
        int start = args.size() > 2 ? int(val(2).asNumber()) - 1 : 0;
        if (start < 0)
            start = 0;
        const int pos = within.indexOf(find, start, Qt::CaseInsensitive);
        if (pos < 0)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return FormulaValue::fromNumber(pos + 1);
    }
    if (name == QLatin1String("SUBSTITUTE")) {
        if (args.size() < 3)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        QString text = val(0).toDisplay();
        const QString old = val(1).toDisplay();
        const QString neu = val(2).toDisplay();
        if (old.isEmpty())
            return FormulaValue::fromText(text);
        if (args.size() >= 4) {
            const int inst = int(val(3).asNumber());
            int pos = -1;
            int seen = 0;
            while ((pos = text.indexOf(old, pos + 1)) >= 0) {
                ++seen;
                if (seen == inst) {
                    text.replace(pos, old.size(), neu);
                    break;
                }
            }
            return FormulaValue::fromText(text);
        }
        return FormulaValue::fromText(QString(text).replace(old, neu));
    }
    if (name == QLatin1String("TEXTJOIN")) {
        if (args.size() < 3)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const QString delim = val(0).toDisplay();
        const bool ignoreEmpty = truthy(val(1));
        QStringList parts;
        for (int i = 2; i < args.size(); ++i) {
            if (args.at(i).isRange) {
                QVector<FormulaValue> cells;
                collectRange(argSheet(args.at(i)), args.at(i).r1, args.at(i).c1, args.at(i).r2, args.at(i).c2, &cells);
                for (const FormulaValue &c : cells) {
                    const QString t = c.toDisplay();
                    if (t.isEmpty() && ignoreEmpty)
                        continue;
                    parts.append(t);
                }
            } else {
                const QString t = val(i).toDisplay();
                if (t.isEmpty() && ignoreEmpty)
                    continue;
                parts.append(t);
            }
        }
        return FormulaValue::fromText(parts.join(delim));
    }
    if (name == QLatin1String("TODAY"))
        return FormulaValue::fromNumber(ExcelDate::toSerial(QDate::currentDate()));
    if (name == QLatin1String("NOW"))
        return FormulaValue::fromNumber(ExcelDate::toSerial(QDateTime::currentDateTime()));
    if (name == QLatin1String("YEAR") || name == QLatin1String("MONTH") || name == QLatin1String("DAY")) {
        const QDate d = parseDate(val(0));
        if (!d.isValid())
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        if (name == QLatin1String("YEAR"))
            return FormulaValue::fromNumber(d.year());
        if (name == QLatin1String("MONTH"))
            return FormulaValue::fromNumber(d.month());
        return FormulaValue::fromNumber(d.day());
    }
    if (name == QLatin1String("DATE")) {
        if (args.size() < 3)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const QDate d(int(val(0).asNumber()), int(val(1).asNumber()), int(val(2).asNumber()));
        if (!d.isValid())
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return FormulaValue::fromNumber(ExcelDate::toSerial(d));
    }
    if (name == QLatin1String("WEEKDAY")) {
        const QDate d = parseDate(val(0));
        if (!d.isValid())
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const int type = args.size() > 1 ? int(val(1).asNumber()) : 1;
        const int iso = d.dayOfWeek();
        if (type == 2)
            return FormulaValue::fromNumber(iso);
        if (type == 3)
            return FormulaValue::fromNumber(iso - 1);
        return FormulaValue::fromNumber(iso == 7 ? 1 : iso + 1);
    }
    if (name == QLatin1String("EOMONTH")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const QDate d = parseDate(val(0));
        if (!d.isValid())
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const QDate shifted = d.addMonths(int(val(1).asNumber()));
        const QDate eom(shifted.year(), shifted.month(), shifted.daysInMonth());
        return FormulaValue::fromNumber(ExcelDate::toSerial(eom));
    }
    if (name == QLatin1String("VLOOKUP") || name == QLatin1String("HLOOKUP")) {
        if (args.size() < 3 || !args.at(1).isRange)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const FormulaValue needle = val(0);
        const FormulaArg &tab = args.at(1);
        const int idx = int(val(2).asNumber());
        bool exact = true;
        if (args.size() >= 4)
            exact = !truthy(val(3));
        const bool vert = name == QLatin1String("VLOOKUP");
        const int sh = argSheet(tab);
        if (idx < 1)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        FormulaValue last;
        bool found = false;
        if (vert) {
            if (tab.c1 + idx - 1 > tab.c2)
                return FormulaValue::fromError(QStringLiteral("#REF!"));
            for (int r = tab.r1; r <= tab.r2; ++r) {
                const FormulaValue key = m_lookup(sh, r, tab.c1);
                if (exact) {
                    if (matchCrit(key, needle) || key.toDisplay().compare(needle.toDisplay(), Qt::CaseInsensitive) == 0) {
                        return m_lookup(sh, r, tab.c1 + idx - 1);
                    }
                } else {
                    bool okK = false, okN = false;
                    const double k = key.asNumber(&okK);
                    const double n = needle.asNumber(&okN);
                    if (okK && okN && k <= n) {
                        last = m_lookup(sh, r, tab.c1 + idx - 1);
                        found = true;
                    }
                }
            }
        } else {
            if (tab.r1 + idx - 1 > tab.r2)
                return FormulaValue::fromError(QStringLiteral("#REF!"));
            for (int c = tab.c1; c <= tab.c2; ++c) {
                const FormulaValue key = m_lookup(sh, tab.r1, c);
                if (exact) {
                    if (key.toDisplay().compare(needle.toDisplay(), Qt::CaseInsensitive) == 0)
                        return m_lookup(sh, tab.r1 + idx - 1, c);
                } else {
                    bool okK = false, okN = false;
                    const double k = key.asNumber(&okK);
                    const double n = needle.asNumber(&okN);
                    if (okK && okN && k <= n) {
                        last = m_lookup(sh, tab.r1 + idx - 1, c);
                        found = true;
                    }
                }
            }
        }
        if (!exact && found)
            return last;
        return FormulaValue::fromError(QStringLiteral("#N/A"));
    }
    if (name == QLatin1String("INDEX")) {
        if (args.isEmpty() || !args.at(0).isRange)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const FormulaArg &rng = args.at(0);
        const int row = args.size() > 1 ? int(val(1).asNumber()) : 1;
        const int col = args.size() > 2 ? int(val(2).asNumber()) : 1;
        if (row < 1 || col < 1)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const int r = rng.r1 + row - 1;
        const int c = rng.c1 + col - 1;
        if (r > rng.r2 || c > rng.c2)
            return FormulaValue::fromError(QStringLiteral("#REF!"));
        return m_lookup(argSheet(rng), r, c);
    }
    if (name == QLatin1String("MATCH")) {
        if (args.size() < 2 || !args.at(1).isRange)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const FormulaValue needle = val(0);
        QVector<FormulaValue> cells;
        collectRange(argSheet(args.at(1)), args.at(1).r1, args.at(1).c1, args.at(1).r2, args.at(1).c2, &cells);
        const int matchType = args.size() > 2 ? int(val(2).asNumber()) : 1;
        if (matchType == 0) {
            for (int i = 0; i < cells.size(); ++i) {
                if (cells.at(i).toDisplay().compare(needle.toDisplay(), Qt::CaseInsensitive) == 0)
                    return FormulaValue::fromNumber(i + 1);
            }
            return FormulaValue::fromError(QStringLiteral("#N/A"));
        }
        int last = -1;
        bool okN = false;
        const double n = needle.asNumber(&okN);
        for (int i = 0; i < cells.size(); ++i) {
            bool okC = false;
            const double c = cells.at(i).asNumber(&okC);
            if (matchType == 1) {
                if (okN && okC && c <= n)
                    last = i;
                else if (!okN && QString::compare(cells.at(i).toDisplay(), needle.toDisplay(), Qt::CaseInsensitive) <= 0)
                    last = i;
            } else {
                if (okN && okC && c >= n)
                    last = i;
                else if (!okN && QString::compare(cells.at(i).toDisplay(), needle.toDisplay(), Qt::CaseInsensitive) >= 0)
                    last = i;
            }
        }
        if (last < 0)
            return FormulaValue::fromError(QStringLiteral("#N/A"));
        return FormulaValue::fromNumber(last + 1);
    }
    if (name == QLatin1String("CHOOSE")) {
        if (args.size() < 2)
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        const int i = int(val(0).asNumber());
        if (i < 1 || i >= args.size())
            return FormulaValue::fromError(QStringLiteral("#VALUE!"));
        return val(i);
    }
    if (name == QLatin1String("ISBLANK"))
        return FormulaValue::fromBool(val(0).isEmpty());
    if (name == QLatin1String("ISNUMBER"))
        return FormulaValue::fromBool(val(0).kind == FormulaValue::Number);
    if (name == QLatin1String("ISTEXT"))
        return FormulaValue::fromBool(val(0).kind == FormulaValue::Text);
    if (name == QLatin1String("ISERROR"))
        return FormulaValue::fromBool(val(0).isError());
    if (name == QLatin1String("N")) {
        bool ok = false;
        const double n = val(0).asNumber(&ok);
        return FormulaValue::fromNumber(ok ? n : 0);
    }
    if (name == QLatin1String("NA"))
        return FormulaValue::fromError(QStringLiteral("#N/A"));
    if (auto *bridge = pluginBridge())
        return (*bridge)(name, args);
    return FormulaValue::fromError(QStringLiteral("#NAME?"));
}
