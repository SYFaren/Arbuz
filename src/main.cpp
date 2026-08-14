#include "appsettings.h"
#include "arbuzicon.h"
#include "firstrunwizard.h"
#include "mainwindow.h"
#include "selftest.h"
#include "theme.h"
#include "uitest.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    qputenv("UBUNTU_MENUPROXY", "0");

    const QByteArray mode = (argc > 1) ? QByteArray(argv[1]) : QByteArray();
    const bool self = mode == QByteArray("--self-test") || mode == QByteArray("--all-test");
    const bool ui = mode == QByteArray("--ui-test") || mode == QByteArray("--all-test");

    if (self || ui) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QApplication app(argc, argv);
        app.setAttribute(Qt::AA_DontShowIconsInMenus, false);
        app.setApplicationName(QStringLiteral("Arbuz"));
        app.setOrganizationName(QStringLiteral("SYFaren"));
        int rc = 0;
        if (self)
            rc = runSelfTest();
        if (rc == 0 && ui) {
            qputenv("ARBUZ_NO_PLUGINS", "1");
            Theme::applyPreset(QStringLiteral("white"));
            Theme::apply(&app);
            rc = runUiTest();
        }
        return rc;
    }

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_DontShowIconsInMenus, false);
    app.setApplicationName(QStringLiteral("Arbuz"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("SYFaren"));
    app.setOrganizationDomain(QStringLiteral("syfaren"));
    app.setWindowIcon(ArbuzIcon::app());

    Theme::apply(&app);

    if (!AppSettings::instance().firstRunDone()) {
        FirstRunWizard wizard;
        wizard.exec();
        Theme::apply(&app);
    }

    MainWindow w;
    w.show();
    return app.exec();
}
