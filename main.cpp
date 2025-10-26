#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCoreApplication>
#include <QtWebEngineQuick/QtWebEngineQuick>
#include <QDebug>
#include "TidalController.h"
#include "MediaController.h"
#include "VoiceAssistant.h"
#include "NotificationManager.h"

void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        abort();
    }
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(myMessageHandler);
    qputenv("QML_XHR_ALLOW_FILE_READ", "1");
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--use-angle=d3d11 --ignore-gpu-blocklist --widevine-path=\"C:/Dev/HeadUnit/widevine/widevinecdm.dll\" --disable-web-security --disable-site-isolation-trials");
    qputenv("QSG_RHI_BACKEND", "d3d11");
    qputenv("QTWEBENGINE_LOGGING", "1");

    QCoreApplication::setOrganizationName("TruckLabs");
    QCoreApplication::setOrganizationDomain("truck.app");
    QCoreApplication::setApplicationName("HeadUnit");

    QGuiApplication app(argc, argv);
    QtWebEngineQuick::initialize();

    // Create all controllers
    TidalController tidalController;
    MediaController mediaController;
    VoiceAssistant voiceAssistant;
    NotificationManager notificationManager;

    QQmlApplicationEngine engine;

    // Expose all controllers to QML
    engine.rootContext()->setContextProperty("tidalController", &tidalController);
    engine.rootContext()->setContextProperty("mediaController", &mediaController);
    engine.rootContext()->setContextProperty("voiceAssistant", &voiceAssistant);
    engine.rootContext()->setContextProperty("notificationManager", &notificationManager);

    // Load QML - version compatible approach
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Qt 6.5+ (including 6.9) - use loadFromModule
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() {
                         qCritical() << "QML Engine Creation Failed";
                         QCoreApplication::exit(-1);
                     },
                     Qt::QueuedConnection);

    engine.loadFromModule("HeadUnit", "Main");
#else
    // Qt 6.2-6.4 - use direct load
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/HeadUnit/Main.qml")));
#endif

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "No root objects created";
        return -1;
    }

    qDebug() << "=== HeadUnit Started Successfully ===";
    qDebug() << "Controllers initialized:";
    qDebug() << "  - Tidal:         " << &tidalController;
    qDebug() << "  - Media:         " << &mediaController;
    qDebug() << "  - Voice:         " << &voiceAssistant;
    qDebug() << "  - Notifications: " << &notificationManager;

    return app.exec();
}
