#ifndef ARBUZ_FORMULAENGINE_H
#define ARBUZ_FORMULAENGINE_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

struct FormulaValue {
    enum Kind { Empty, Number, Text, Bool, Error };
    Kind kind = Empty;
    double number = 0;
    QString text;
    bool boolean = false;
    QString error;

    static FormulaValue fromNumber(double n)
    {
        FormulaValue v;
        v.kind = Number;
        v.number = n;
        return v;
    }
    static FormulaValue fromText(const QString &t)
    {
        FormulaValue v;
        v.kind = Text;
        v.text = t;
        return v;
    }
    static FormulaValue fromBool(bool b)
    {
        FormulaValue v;
        v.kind = Bool;
        v.boolean = b;
        v.number = b ? 1 : 0;
        return v;
    }
    static FormulaValue fromError(const QString &e)
    {
        FormulaValue v;
        v.kind = Error;
        v.error = e;
        return v;
    }

    bool isError() const { return kind == Error; }
    bool isEmpty() const { return kind == Empty || (kind == Text && text.isEmpty()); }
    QString toDisplay() const;
    double asNumber(bool *ok = nullptr) const;
};

struct FormulaArg {
    bool isRange = false;
    int sheet = -1;
    int r1 = 0;
    int c1 = 0;
    int r2 = 0;
    int c2 = 0;
    FormulaValue value;
};

class FormulaEngine
{
public:
    using CellLookup = std::function<FormulaValue(int sheet, int row, int col)>;
    using SheetLookup = std::function<int(const QString &name)>;

    explicit FormulaEngine(CellLookup lookup, int currentSheet, SheetLookup sheets = {});

    struct FormulaInfo {
        QString name;
        QString syntax;
        QString category;
        /** i18n key for built-in help, e.g. "fn.SUM.help". Empty for plugins. */
        QString helpKey;
        QString helpRuOverride;
        QString helpEnOverride;
        QString help() const;
        QString helpRu() const;
        QString helpEn() const;
    };

    FormulaValue evaluate(const QString &formula);
    static QStringList dependencyRefs(const QString &formula);
    static QVector<FormulaInfo> catalog();
    static QStringList categories();

    using PluginBridge = std::function<FormulaValue(const QString &name, const QVector<FormulaArg> &args)>;
    static void setPluginBridge(PluginBridge fn, const QVector<FormulaInfo> &extra);
    static QVector<FormulaInfo> pluginCatalog();

private:
    CellLookup m_lookup;
    SheetLookup m_sheets;
    int m_sheet = 0;
    QString m_src;
    int m_pos = 0;

    void skipWs();
    bool match(QChar c);
    bool matchSep();
    FormulaValue parseExpr();
    FormulaValue parseCompare();
    FormulaValue parseConcat();
    FormulaValue parseAdd();
    FormulaValue parseMul();
    FormulaValue parsePower();
    FormulaValue parseUnary();
    FormulaValue parsePrimary();
    FormulaValue parseFunction(const QString &name);
    QVector<FormulaArg> parseArgs();
    int resolveSheet(const QString &name) const;
    int argSheet(const FormulaArg &a) const;
    void collectRange(int sheet, int r1, int c1, int r2, int c2, QVector<FormulaValue> *out) const;
    QVector<FormulaValue> flatten(const QVector<FormulaArg> &args) const;
};

#endif
