#ifndef ARBUZ_PLUGINHOST_H
#define ARBUZ_PLUGINHOST_H

#include "formulaengine.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVector>

class Workbook;
class QProcess;
class QMenu;

class PluginHost : public QObject
{
    Q_OBJECT
public:
    struct Command {
        QString id;
        QString titleRu;
        QString titleEn;
        QString pluginId;
    };

    static PluginHost &instance();

    void start(Workbook *workbook);
    void stop();
    void reload();
    bool running() const;
    QString pythonPath() const { return m_python; }
    QString pluginsDir() const;
    QStringList loadedPlugins() const { return m_loaded; }
    QVector<Command> commands() const { return m_commands; }
    QString statusMessage() const { return m_status; }

    void fillMenu(QMenu *menu);
    void setCurrentSheet(int sheet);
    void setSelection(const QString &a1, const QString &range);
    void notifyCellEdited(int sheet, int row, int col);
    void notifyEvent(const QString &name, const QJsonObject &extra = QJsonObject());

    FormulaValue evalFunction(const QString &name, const QVector<FormulaArg> &args);

signals:
    void requestSheetChange(int index);

private:
    explicit PluginHost(QObject *parent = nullptr);
    QString findPython() const;
    QString findRunner() const;
    QString portableRoot() const;
    int resolveSheet(const QJsonObject &req) const;
    QJsonObject rpc(const QJsonObject &req, int timeoutMs = 8000);
    void handleHostLine(const QByteArray &line);
    void applyRegistrations(const QJsonObject &msg);
    void serveRequest(const QJsonObject &req);
    void send(const QJsonObject &obj);
    QJsonValue argToJson(const FormulaArg &a) const;
    FormulaValue jsonToValue(const QJsonObject &o) const;
    QJsonValue cellJson(int sheet, int row, int col, bool raw) const;

    QPointer<Workbook> m_wb;
    QProcess *m_proc = nullptr;
    QString m_python;
    QString m_status;
    QStringList m_loaded;
    QVector<Command> m_commands;
    QVector<FormulaEngine::FormulaInfo> m_fns;
    QByteArray m_buf;
    QHash<QString, FormulaValue> m_fnCache;
    bool m_inCall = false;
    bool m_serving = false;
    int m_sheet = 0;
    QString m_currentA1 = QStringLiteral("A1");
    QString m_selectionRange = QStringLiteral("A1");
};

#endif
