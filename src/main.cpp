#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QWindow>

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include "app/traycontroller.h"

using namespace Qt::Literals::StringLiterals;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // These decide where QSettings and QStandardPaths put things, so they are
    // part of the app's on-disk contract. KVault is not a KDE project, so it
    // uses its own reverse-DNS namespace rather than org.kde.
    QCoreApplication::setOrganizationName(u"io.github.timpalpant"_s);
    QCoreApplication::setOrganizationDomain(u"timpalpant.github.io"_s);
    QCoreApplication::setApplicationName(u"kvault"_s);
    QGuiApplication::setDesktopFileName(u"io.github.timpalpant.kvault"_s);

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kvault"));

    // Without this the app falls back to the plain Qt Quick style and looks
    // nothing like the rest of the desktop.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    KAboutData about(u"kvault"_s, i18n("KVault"), u"0.1.0"_s, i18n("An unofficial client for Bitwarden servers"), KAboutLicense::GPL_V3);
    about.addAuthor(i18n("Tim Palpant"), {}, u"tim@palpant.us"_s);
    about.setHomepage(u"https://github.com/timpalpant/kvault"_s);
    // KAboutData defaults this to "org.kde." + componentName, and
    // setApplicationData() pushes it into QGuiApplication, overwriting the id
    // set above. Left unset, the desktop portal looks for org.kde.kvault, fails
    // to find it, and the failed registration stalls startup.
    about.setDesktopFileName(u"io.github.timpalpant.kvault"_s);
    KAboutData::setApplicationData(about);

    // The D-Bus service makes a second launch raise this process rather than
    // creating another vault session. Without a session bus we still run, just
    // without single-instance coordination.
    KDBusService dbusService(KDBusService::Unique | KDBusService::NoExitOnFailure);
    if (!dbusService.isRegistered()) {
        const QDBusConnection sessionBus = QDBusConnection::sessionBus();
        const auto *busInterface = sessionBus.interface();
        const auto existingService = busInterface
            ? busInterface->isServiceRegistered(u"io.github.timpalpant.kvault"_s)
            : QDBusReply<bool>();
        if (sessionBus.isConnected() && existingService.isValid() && existingService.value()) {
            return 0;
        }
    }

    QGuiApplication::setWindowIcon(
        QIcon::fromTheme(u"io.github.timpalpant.kvault"_s, QIcon(u":/qt/qml/io/github/timpalpant/kvault/src/qml/icons/kvault.svg"_s)));

    kvault::TrayController trayController;
    QObject::connect(&dbusService, &KDBusService::activateRequested, &trayController,
        [&trayController](const QStringList &, const QString &) {
            trayController.showMainWindow();
        });

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.rootContext()->setContextProperty(u"TrayController"_s, &trayController);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("io.github.timpalpant.kvault", "Main");
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    trayController.setMainWindow(qobject_cast<QWindow *>(engine.rootObjects().constFirst()));

    return app.exec();
}
