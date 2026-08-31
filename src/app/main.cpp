#include "appsettings.h"
#include "arbuzicon.h"
#include "firstrunwizard.h"
#include "mainwindow.h"
#include "selftest.h"
#include "demo.h"
#include "theme.h"
#include "uitest.h"

#include <QApplication>
#include <QCoreApplication>
#include <cstdio>

int main(int argc, char *argv[])
{
    qputenv("UBUNTU_MENUPROXY", "0");

    const QByteArray mode = (argc > 1) ? QByteArray(argv[1]) : QByteArray();
    const bool self = mode == QByteArray("--self-test") || mode == QByteArray("--all-test");
    const bool ui = mode == QByteArray("--ui-test") || mode == QByteArray("--all-test");
    const bool writeDemo = mode == QByteArray("--write-demo");
    const bool openDemo = mode == QByteArray("--open-demo");

    if (writeDemo) {
        QCoreApplication app(argc, argv);
        if (argc < 3) {
            std::fprintf(stderr, "usage: arbuz --write-demo path.xlsx\n");
            return 2;
        }
        return writeDemoWorkbook(QString::fromLocal8Bit(argv[2]));
    }

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
    if (openDemo) {
        w.openDemo();
    } else {
        for (int i = 1; i < argc; ++i) {
            const QByteArray a(argv[i]);
            if (a.startsWith('-'))
                continue;
            w.openPath(QString::fromLocal8Bit(argv[i]));
            break;
        }
    }
    w.show();
    return app.exec();
}
