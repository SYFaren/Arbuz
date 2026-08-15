#include "pluginhost.h"
#include "cellref.h"
#include "i18n.h"
#include "numformat.h"
#include "portable.h"
#include "workbook.h"

#include <QAction>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMenu>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUrl>
#include <utility>

PluginHost &PluginHost::instance()
{
    static PluginHost h;
    return h;
}

PluginHost::PluginHost(QObject *parent)
    : QObject(parent)
{
}

QString PluginHost::portableRoot() const
{
    return Arbuz::portableRoot();
}

QString PluginHost::pluginsDir() const
{
    const QString root = portableRoot();
    const QString nextToApp = root + QStringLiteral("/plugins");
    const QString legacy = root + QStringLiteral("/python-plugins");
    if (QDir(nextToApp).exists())
        return nextToApp;
    if (QDir(legacy).exists())
        return legacy;
    const QString cfgBase = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString cfg = cfgBase + QStringLiteral("/plugins");
    const QString cfgLegacy = cfgBase + QStringLiteral("/python-plugins");
    if (QDir(cfgLegacy).exists() && !QDir(cfg).exists())
        return cfgLegacy;
    QDir().mkpath(cfg);
    return cfg;
}

QString PluginHost::findPython() const
{
    const QString env = QString::fromLocal8Bit(qgetenv("ARBUZ_PYTHON"));
    if (!env.isEmpty() && QFileInfo::exists(env))
        return env;
#ifdef Q_OS_WIN
    const QStringList names = {QStringLiteral("python.exe"), QStringLiteral("python3.exe"),
                               QStringLiteral("py.exe")};
#else
    const QStringList names = {QStringLiteral("python3"), QStringLiteral("python")};
#endif
    for (const QString &n : names) {
        const QString p = QStandardPaths::findExecutable(n);
        if (!p.isEmpty())
            return p;
    }
    return {};
}

QString PluginHost::findRunner() const
{
    QDir app(QCoreApplication::applicationDirPath());
    const QString root = portableRoot();
    const QStringList candidates = {
        app.filePath(QStringLiteral("python-host/plugin_runner.py")),
        app.filePath(QStringLiteral("runtime/python-host/plugin_runner.py")),
        root + QStringLiteral("/runtime/python-host/plugin_runner.py"),
        app.filePath(QStringLiteral("../python-host/plugin_runner.py")),
        app.filePath(QStringLiteral("../python/plugin_runner.py")),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../python/plugin_runner.py"),
    };
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c))
            return QFileInfo(c).absoluteFilePath();
    }
    return {};
}

void PluginHost::send(const QJsonObject &obj)
{
    if (!m_proc)
        return;
    m_proc->write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    m_proc->write("\n");
    m_proc->waitForBytesWritten(200);
}

void PluginHost::start(Workbook *workbook)
{
    m_wb = workbook;
    if (qEnvironmentVariableIntValue("ARBUZ_NO_PLUGINS") > 0) {
        m_status = QStringLiteral("disabled");
        return;
    }
    reload();
    notifyEvent(QStringLiteral("app_start"));
}

void PluginHost::stop()
{
    FormulaEngine::setPluginBridge({}, {});
    m_fns.clear();
    m_commands.clear();
    m_loaded.clear();
    m_fnCache.clear();
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(500);
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void PluginHost::reload()
{
    stop();
    m_python = findPython();
    const QString runner = findRunner();
    const QString plugDir = pluginsDir();
    if (m_python.isEmpty() || runner.isEmpty()) {
        m_status = m_python.isEmpty() ? QStringLiteral("no-python") : QStringLiteral("no-runner");
        return;
    }

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString hostDir = QFileInfo(runner).absolutePath();
    env.insert(QStringLiteral("PYTHONPATH"), hostDir);
    env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    env.insert(QStringLiteral("ARBUZ_PLUGINS_DIR"), plugDir);
    m_proc->setProcessEnvironment(env);
    connect(m_proc, &QProcess::readyRead, this, [this]() {
        m_buf += m_proc->readAll();
        if (m_inCall)
            return;
        while (true) {
            const int nl = m_buf.indexOf('\n');
            if (nl < 0)
                break;
            const QByteArray line = m_buf.left(nl).trimmed();
            m_buf.remove(0, nl + 1);
            if (!line.isEmpty())
                handleHostLine(line);
        }
    });
    m_proc->start(m_python, {runner, plugDir});
    if (!m_proc->waitForStarted(3000)) {
        m_status = QStringLiteral("start-failed");
        stop();
        return;
    }
    if (!m_proc->waitForReadyRead(4000) && m_loaded.isEmpty()) {
        m_status = QStringLiteral("timeout");
        return;
    }
    m_status = m_loaded.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ok");
}

bool PluginHost::running() const
{
    return m_proc && m_proc->state() == QProcess::Running;
}

void PluginHost::setCurrentSheet(int sheet)
{
    if (m_sheet == sheet)
        return;
    m_sheet = sheet;
    notifyEvent(QStringLiteral("sheet_changed"), QJsonObject{{QStringLiteral("sheet"), sheet}});
}

void PluginHost::setSelection(const QString &a1, const QString &range)
{
    const QString cell = a1.isEmpty() ? QStringLiteral("A1") : a1;
    const QString rng = range.isEmpty() ? cell : range;
    if (cell == m_currentA1 && rng == m_selectionRange)
        return;
    m_currentA1 = cell;
    m_selectionRange = rng;
    notifyEvent(QStringLiteral("selection_changed"),
                QJsonObject{{QStringLiteral("a1"), m_currentA1},
                            {QStringLiteral("range"), m_selectionRange}});
}

void PluginHost::handleHostLine(const QByteArray &line)
{
    const QJsonObject o = QJsonDocument::fromJson(line).object();
    const QString op = o.value(QStringLiteral("op")).toString();
    if (op == QLatin1String("ready") || op == QLatin1String("register")) {
        applyRegistrations(o);
        return;
    }
    if (op == QLatin1String("request") && !m_inCall)
        serveRequest(o);
}

void PluginHost::applyRegistrations(const QJsonObject &msg)
{
    if (msg.contains(QStringLiteral("plugins"))) {
        m_loaded.clear();
        for (const auto &v : msg.value(QStringLiteral("plugins")).toArray())
            m_loaded.append(v.toString());
    }
    if (msg.contains(QStringLiteral("functions"))) {
        m_fns.clear();
        for (const auto &v : msg.value(QStringLiteral("functions")).toArray()) {
            const QJsonObject f = v.toObject();
            FormulaEngine::FormulaInfo info;
            info.name = f.value(QStringLiteral("name")).toString().toUpper();
            info.syntax = f.value(QStringLiteral("syntax")).toString();
            if (info.syntax.isEmpty())
                info.syntax = info.name + QStringLiteral("()");
            info.category = QStringLiteral("plugin");
            info.helpRuOverride = f.value(QStringLiteral("help_ru")).toString();
            info.helpEnOverride = f.value(QStringLiteral("help_en")).toString();
            m_fns.append(info);
        }
    }
    if (msg.contains(QStringLiteral("commands"))) {
        m_commands.clear();
        for (const auto &v : msg.value(QStringLiteral("commands")).toArray()) {
            const QJsonObject c = v.toObject();
            Command cmd;
            cmd.id = c.value(QStringLiteral("id")).toString();
            cmd.titleRu = c.value(QStringLiteral("title_ru")).toString();
            cmd.titleEn = c.value(QStringLiteral("title_en")).toString();
            cmd.pluginId = c.value(QStringLiteral("plugin")).toString();
            m_commands.append(cmd);
        }
    }
    FormulaEngine::setPluginBridge(
        [this](const QString &name, const QVector<FormulaArg> &args) { return evalFunction(name, args); },
        m_fns);
    if (msg.value(QStringLiteral("op")).toString() == QLatin1String("ready"))
        m_status = QStringLiteral("ok");
}

int PluginHost::resolveSheet(const QJsonObject &req) const
{
    int sh = req.contains(QStringLiteral("sheet")) ? req.value(QStringLiteral("sheet")).toInt() : m_sheet;
    if (!m_wb || sh < 0 || sh >= m_wb->sheetCount())
        return m_sheet;
    return sh;
}

QJsonValue PluginHost::cellJson(int sheet, int row, int col, bool raw) const
{
    if (!m_wb)
        return QString();
    if (raw)
        return m_wb->sheet(sheet).cell(row, col).raw;
    const QString d = m_wb->displayText(sheet, row, col);
    double n = 0;
    if (NumFormat::parse(d, &n))
        return n;
    if (d.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0)
        return true;
    if (d.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0)
        return false;
    return d;
}

void PluginHost::serveRequest(const QJsonObject &req)
{
    const int id = req.value(QStringLiteral("id")).toInt();
    const QString method = req.value(QStringLiteral("method")).toString();
    QJsonObject out{{QStringLiteral("op"), QStringLiteral("reply")}, {QStringLiteral("id"), id}};
    m_serving = true;
    if (!m_wb) {
        out.insert(QStringLiteral("error"), QStringLiteral("no workbook"));
        m_serving = false;
        send(out);
        return;
    }
    if (method == QLatin1String("get")) {
        int r = 0, c = 0;
        CellRef::parseA1(req.value(QStringLiteral("a1")).toString(), &r, &c);
        const int sh = resolveSheet(req);
        out.insert(QStringLiteral("value"), m_wb->displayText(sh, r, c));
        out.insert(QStringLiteral("raw"), m_wb->sheet(sh).cell(r, c).raw);
        out.insert(QStringLiteral("a1"), CellRef::a1(r, c));
        out.insert(QStringLiteral("sheet"), sh);
    } else if (method == QLatin1String("set")) {
        int r = 0, c = 0;
        CellRef::parseA1(req.value(QStringLiteral("a1")).toString(), &r, &c);
        m_wb->setRaw(resolveSheet(req), r, c, req.value(QStringLiteral("value")).toVariant().toString());
        out.insert(QStringLiteral("ok"), true);
    } else if (method == QLatin1String("get_range")) {
        int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
        if (!CellRef::parseA1Range(req.value(QStringLiteral("a1")).toString(), &r1, &c1, &r2, &c2)) {
            out.insert(QStringLiteral("error"), QStringLiteral("bad range"));
        } else {
            if (r1 > r2)
                std::swap(r1, r2);
            if (c1 > c2)
                std::swap(c1, c2);
            const int sh = resolveSheet(req);
            const bool raw = req.value(QStringLiteral("raw")).toBool();
            QJsonArray rows;
            const int cap = 20000;
            int n = 0;
            for (int r = r1; r <= r2 && n < cap; ++r) {
                QJsonArray row;
                for (int c = c1; c <= c2 && n < cap; ++c, ++n)
                    row.append(cellJson(sh, r, c, raw));
                rows.append(row);
            }
            out.insert(QStringLiteral("values"), rows);
            out.insert(QStringLiteral("a1"),
                       CellRef::a1(r1, c1) + QLatin1Char(':') + CellRef::a1(r2, c2));
        }
    } else if (method == QLatin1String("set_range")) {
        int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
        if (!CellRef::parseA1Range(req.value(QStringLiteral("a1")).toString(), &r1, &c1, &r2, &c2)) {
            out.insert(QStringLiteral("error"), QStringLiteral("bad range"));
        } else {
            if (r1 > r2)
                std::swap(r1, r2);
            if (c1 > c2)
                std::swap(c1, c2);
            const int sh = resolveSheet(req);
            const QJsonArray values = req.value(QStringLiteral("values")).toArray();
            int written = 0;
            for (int i = 0; i < values.size(); ++i) {
                const QJsonValue rowv = values.at(i);
                if (rowv.isArray()) {
                    const QJsonArray row = rowv.toArray();
                    for (int j = 0; j < row.size(); ++j) {
                        m_wb->setRaw(sh, r1 + i, c1 + j, row.at(j).toVariant().toString());
                        ++written;
                    }
                } else {
                    const bool down = (r2 - r1) >= (c2 - c1);
                    if (down)
                        m_wb->setRaw(sh, r1 + i, c1, rowv.toVariant().toString());
                    else
                        m_wb->setRaw(sh, r1, c1 + i, rowv.toVariant().toString());
                    ++written;
                }
            }
            out.insert(QStringLiteral("ok"), true);
            out.insert(QStringLiteral("written"), written);
        }
    } else if (method == QLatin1String("sheets")) {
        QJsonArray arr;
        for (int i = 0; i < m_wb->sheetCount(); ++i)
            arr.append(m_wb->sheet(i).name);
        out.insert(QStringLiteral("sheets"), arr);
        out.insert(QStringLiteral("count"), m_wb->sheetCount());
        out.insert(QStringLiteral("current"), m_sheet);
    } else if (method == QLatin1String("current_sheet")) {
        out.insert(QStringLiteral("sheet"), m_sheet);
        if (m_sheet >= 0 && m_sheet < m_wb->sheetCount())
            out.insert(QStringLiteral("name"), m_wb->sheet(m_sheet).name);
    } else if (method == QLatin1String("set_current_sheet")) {
        const int sh = req.value(QStringLiteral("sheet")).toInt();
        if (sh < 0 || sh >= m_wb->sheetCount()) {
            out.insert(QStringLiteral("error"), QStringLiteral("bad sheet"));
        } else {
            emit requestSheetChange(sh);
            out.insert(QStringLiteral("ok"), true);
        }
    } else if (method == QLatin1String("add_sheet")) {
        const int idx = m_wb->addSheet(req.value(QStringLiteral("name")).toString());
        out.insert(QStringLiteral("sheet"), idx);
        out.insert(QStringLiteral("ok"), true);
    } else if (method == QLatin1String("rename_sheet")) {
        const int sh = req.contains(QStringLiteral("sheet")) ? req.value(QStringLiteral("sheet")).toInt() : m_sheet;
        const bool ok = m_wb->renameSheet(sh, req.value(QStringLiteral("name")).toString());
        if (!ok)
            out.insert(QStringLiteral("error"), QStringLiteral("rename failed"));
        else
            out.insert(QStringLiteral("ok"), true);
    } else if (method == QLatin1String("remove_sheet")) {
        const int sh = req.contains(QStringLiteral("sheet")) ? req.value(QStringLiteral("sheet")).toInt() : m_sheet;
        const bool ok = m_wb->removeSheet(sh);
        if (!ok)
            out.insert(QStringLiteral("error"), QStringLiteral("remove failed"));
        else
            out.insert(QStringLiteral("ok"), true);
    } else if (method == QLatin1String("current_cell")) {
        out.insert(QStringLiteral("a1"), m_currentA1);
        out.insert(QStringLiteral("sheet"), m_sheet);
    } else if (method == QLatin1String("selection")) {
        out.insert(QStringLiteral("a1"), m_currentA1);
        out.insert(QStringLiteral("range"), m_selectionRange);
        out.insert(QStringLiteral("sheet"), m_sheet);
    } else if (method == QLatin1String("row_count")) {
        const int sh = resolveSheet(req);
        out.insert(QStringLiteral("rows"), m_wb->sheet(sh).rowCount);
    } else if (method == QLatin1String("col_count")) {
        const int sh = resolveSheet(req);
        out.insert(QStringLiteral("cols"), m_wb->sheet(sh).colCount);
    } else {
        out.insert(QStringLiteral("error"), QStringLiteral("unknown method"));
    }
    m_serving = false;
    send(out);
}

QJsonValue PluginHost::argToJson(const FormulaArg &a) const
{
    QJsonObject o;
    if (a.isRange) {
        o.insert(QStringLiteral("range"),
                 CellRef::a1(a.r1, a.c1) + QLatin1Char(':') + CellRef::a1(a.r2, a.c2));
        QJsonArray rows;
        if (m_wb) {
            const int sh = (a.sheet >= 0 && a.sheet < m_wb->sheetCount()) ? a.sheet : m_sheet;
            o.insert(QStringLiteral("sheet"), sh);
            int rLo = qMin(a.r1, a.r2), rHi = qMax(a.r1, a.r2);
            int cLo = qMin(a.c1, a.c2), cHi = qMax(a.c1, a.c2);
            int lastR = -1, lastC = -1;
            if (!m_wb->usedCorner(sh, &lastR, &lastC)) {
                o.insert(QStringLiteral("values"), QJsonArray());
                o.insert(QStringLiteral("rows"), rows);
                return o;
            }
            rHi = qMin(rHi, lastR);
            cHi = qMin(cHi, lastC);
            QJsonArray flat;
            if (rLo <= rHi && cLo <= cHi) {
                for (int r = rLo; r <= rHi; ++r) {
                    QJsonArray row;
                    for (int c = cLo; c <= cHi; ++c) {
                        const QJsonValue v = cellJson(sh, r, c, false);
                        row.append(v);
                        flat.append(v);
                    }
                    rows.append(row);
                }
            }
            o.insert(QStringLiteral("values"), flat);
            o.insert(QStringLiteral("rows"), rows);
        }
        return o;
    }
    if (a.value.kind == FormulaValue::Number)
        return a.value.number;
    if (a.value.kind == FormulaValue::Bool)
        return a.value.boolean;
    if (a.value.kind == FormulaValue::Error)
        return QJsonObject{{QStringLiteral("error"), a.value.error}};
    return a.value.toDisplay();
}

FormulaValue PluginHost::jsonToValue(const QJsonObject &o) const
{
    if (o.contains(QStringLiteral("error")) && !o.contains(QStringLiteral("value")))
        return FormulaValue::fromError(o.value(QStringLiteral("error")).toString());
    const QJsonValue v = o.value(QStringLiteral("value"));
    if (v.isObject()) {
        const QJsonObject inner = v.toObject();
        if (inner.contains(QStringLiteral("error")))
            return FormulaValue::fromError(inner.value(QStringLiteral("error")).toString());
    }
    if (v.isDouble())
        return FormulaValue::fromNumber(v.toDouble());
    if (v.isBool())
        return FormulaValue::fromBool(v.toBool());
    if (v.isNull() || v.isUndefined())
        return FormulaValue();
    return FormulaValue::fromText(v.toVariant().toString());
}

QJsonObject PluginHost::rpc(const QJsonObject &req, int timeoutMs)
{
    if (!running())
        return {{QStringLiteral("error"), QStringLiteral("#NAME?")}};
    m_inCall = true;
    send(req);
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < timeoutMs) {
        m_buf += m_proc->readAll();
        while (true) {
            const int nl = m_buf.indexOf('\n');
            if (nl < 0)
                break;
            const QByteArray line = m_buf.left(nl).trimmed();
            m_buf.remove(0, nl + 1);
            const QJsonObject o = QJsonDocument::fromJson(line).object();
            const QString op = o.value(QStringLiteral("op")).toString();
            if (op == QLatin1String("request")) {
                serveRequest(o);
                continue;
            }
            if (op == QLatin1String("result")
                && o.value(QStringLiteral("id")).toInt() == req.value(QStringLiteral("id")).toInt()) {
                m_inCall = false;
                return o;
            }
            handleHostLine(line);
        }
        if (!m_proc->waitForReadyRead(qMin(200, timeoutMs - int(t.elapsed())))) {
            if (m_proc->state() != QProcess::Running)
                break;
        }
    }
    m_inCall = false;
    return {{QStringLiteral("error"), QStringLiteral("#TIMEOUT!")}};
}

FormulaValue PluginHost::evalFunction(const QString &name, const QVector<FormulaArg> &args)
{
    static int reqId = 1;
    QJsonArray ja;
    for (const FormulaArg &a : args)
        ja.append(argToJson(a));
    const QString cacheKey = name + QLatin1Char('\n')
        + QString::fromUtf8(QJsonDocument(ja).toJson(QJsonDocument::Compact));
    if (m_fnCache.contains(cacheKey))
        return m_fnCache.value(cacheKey);
    if (m_inCall || m_serving)
        return FormulaValue::fromError(QStringLiteral("#BUSY"));
    QJsonObject req{{QStringLiteral("op"), QStringLiteral("call_function")},
                    {QStringLiteral("id"), reqId++},
                    {QStringLiteral("name"), name},
                    {QStringLiteral("args"), ja}};
    const QJsonObject res = rpc(req);
    FormulaValue v;
    if (res.contains(QStringLiteral("error")) && !res.contains(QStringLiteral("value")))
        v = FormulaValue::fromError(res.value(QStringLiteral("error")).toString());
    else
        v = jsonToValue(res);
    if (!(v.isError()
          && (v.error == QLatin1String("#TIMEOUT!") || v.error == QLatin1String("#BUSY"))))
        m_fnCache.insert(cacheKey, v);
    return v;
}

void PluginHost::notifyCellEdited(int sheet, int row, int col)
{
    m_fnCache.clear();
    if (!running() || m_inCall || m_serving)
        return;
    send(QJsonObject{{QStringLiteral("op"), QStringLiteral("event")},
                     {QStringLiteral("name"), QStringLiteral("cell_changed")},
                     {QStringLiteral("sheet"), sheet},
                     {QStringLiteral("a1"), CellRef::a1(row, col)}});
}

void PluginHost::notifyEvent(const QString &name, const QJsonObject &extra)
{
    if (!running() || m_serving)
        return;
    QJsonObject o{{QStringLiteral("op"), QStringLiteral("event")}, {QStringLiteral("name"), name}};
    for (auto it = extra.begin(); it != extra.end(); ++it)
        o.insert(it.key(), it.value());
    send(o);
}

void PluginHost::fillMenu(QMenu *menu)
{
    if (!menu)
        return;
    menu->clear();
    auto *reload = menu->addAction(I18n::t("ui.reload_plugins"));
    connect(reload, &QAction::triggered, this, [this]() { this->reload(); });
    auto *open = menu->addAction(I18n::t("ui.open_plugins_folder"));
    connect(open, &QAction::triggered, this, [this]() {
        QDir().mkpath(pluginsDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(pluginsDir()));
    });
    menu->addSeparator();
    if (!running()) {
        QString why = I18n::t("ui.python_3_not_found_plugins_disabled");
        if (m_status == QLatin1String("no-runner"))
            why = I18n::t("ui.plugin_runner_not_found");
        else if (m_status == QLatin1String("disabled"))
            why = I18n::t("ui.plugins_disabled");
        else if (m_status == QLatin1String("start-failed") || m_status == QLatin1String("timeout"))
            why = I18n::t("ui.failed_to_start_python_plugins");
        auto *st = menu->addAction(why);
        st->setEnabled(false);
        return;
    }
    if (m_commands.isEmpty() && m_loaded.isEmpty()) {
        auto *st = menu->addAction(I18n::t("ui.no_plugins_installed"));
        st->setEnabled(false);
        return;
    }
    for (const Command &cmd : m_commands) {
        const QString title = (I18n::lang() == QLatin1String("en") && !cmd.titleEn.isEmpty())
            ? cmd.titleEn
            : (cmd.titleRu.isEmpty() ? cmd.id : cmd.titleRu);
        auto *act = menu->addAction(title.isEmpty() ? cmd.id : title);
        const QString id = cmd.id;
        connect(act, &QAction::triggered, this, [this, id]() {
            static int cid = 1000;
            rpc(QJsonObject{{QStringLiteral("op"), QStringLiteral("run_command")},
                            {QStringLiteral("id"), cid++},
                            {QStringLiteral("command"), id}},
                15000);
        });
    }
    if (!m_loaded.isEmpty()) {
        menu->addSeparator();
        auto *info = menu->addAction(
            I18n::t("ui.loaded_1").arg(m_loaded.join(QStringLiteral(", "))));
        info->setEnabled(false);
    }
}
