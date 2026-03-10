#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QDebug>
#include <QDir>
#include <QUrl>
#include "MediaController.h"
#include "VoiceAssistant.h"
#include "PicovoiceManager.h"
#include "ClaudeClient.h"
#include "GoogleTTS.h"
#include "NotificationManager.h"
#include "BluetoothManager.h"
#include "ContactManager.h"
#include "MessageManager.h"
#include "VoiceCommandHandler.h"
#include "WeatherManager.h"
#include "VehicleBusManager.h"
#include "TidalClient.h"
#include "SpotifyClient.h"
#include "UpdateManager.h"

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
    QCoreApplication::setOrganizationName("TruckLabs");
    QCoreApplication::setOrganizationDomain("truck.app");
    QCoreApplication::setApplicationName("HeadUnit");

    // Set OpenGL rendering backend for MapLibre native rendering
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);

    // Create all controllers
    MediaController mediaController;
    VoiceAssistant voiceAssistant;
    PicovoiceManager picovoiceManager;
    ClaudeClient claudeClient;
    GoogleTTS googleTTS;
    NotificationManager notificationManager;
    BluetoothManager bluetoothManager;
    ContactManager contactManager;
    MessageManager messageManager;
    VoiceCommandHandler voiceCommandHandler;
    WeatherManager weatherManager;
    VehicleBusManager vehicleBusManager;
    TidalClient tidalClient;
    SpotifyClient spotifyClient;
    UpdateManager updateManager;

    // Set API keys from environment variables (loaded via .env)
    googleTTS.setApiKey(qEnvironmentVariable("GOOGLE_API_KEY"));
    picovoiceManager.setAccessKey(qEnvironmentVariable("PICOVOICE_ACCESS_KEY"));
    picovoiceManager.setGoogleApiKey(qEnvironmentVariable("GOOGLE_API_KEY"));

    // Set default TTS preferences (can be changed in Settings)
    googleTTS.setVoiceName("en-US-Neural2-F");  // Female US English neural voice
    googleTTS.setSpeakingRate(1.0);  // Normal speed
    googleTTS.setPitch(0.0);  // Normal pitch

    // Connect wake word detector to QML Claude UI (handled in Main.qml via Connections)
    // Note: The wake word signal will be connected to QML to trigger the Claude AI interface
    // The wakeWordDetected signal is exposed via setContextProperty below

    QQmlApplicationEngine engine;

    // Set up VoiceCommandHandler dependencies
    voiceCommandHandler.setContactManager(&contactManager);
    voiceCommandHandler.setMessageManager(&messageManager);
    voiceCommandHandler.setBluetoothManager(&bluetoothManager);
    voiceCommandHandler.setGoogleTTS(&googleTTS);

    // Set up BluetoothManager dependencies
    bluetoothManager.setContactManager(&contactManager);

    // Connect PicovoiceManager signals to handlers
    // Wake word detected -> triggers QML UI (handled in Main.qml)
    // Transcription ready -> send to Claude AI (using lambda to add empty systemContext)
    QObject::connect(&picovoiceManager, &PicovoiceManager::transcriptionReady,
                     [&claudeClient](const QString &text) {
                         claudeClient.sendMessage(text, QString());  // Empty systemContext
                     });

    // Provide Claude with contact list for intelligent name matching
    // Also provide to PicovoiceManager for Google STT speech context hints
    // Update contacts when sync completes
    QObject::connect(&contactManager, &ContactManager::syncCompleted,
                     [&claudeClient, &contactManager, &picovoiceManager](int /*count*/) {
                         QStringList names = contactManager.getAllContactNames();
                         claudeClient.setContactNames(names);
                         picovoiceManager.setSpeechContextHints(names);
                     });

    // Also set initial contacts if already loaded from cache
    QStringList cachedNames = contactManager.getAllContactNames();
    if (!cachedNames.isEmpty()) {
        claudeClient.setContactNames(cachedNames);
        picovoiceManager.setSpeechContextHints(cachedNames);
    }

    // Note: Claude response -> Google TTS is handled in Main.qml onResponseReceived
    // to coordinate with UI state changes (avoiding duplicate speak() calls)

    // Expose all controllers to QML
    engine.rootContext()->setContextProperty("mediaController", &mediaController);
    engine.rootContext()->setContextProperty("voiceAssistant", &voiceAssistant);
    engine.rootContext()->setContextProperty("picovoiceManager", &picovoiceManager);
    engine.rootContext()->setContextProperty("claudeClient", &claudeClient);
    engine.rootContext()->setContextProperty("googleTTS", &googleTTS);
    engine.rootContext()->setContextProperty("notificationManager", &notificationManager);
    engine.rootContext()->setContextProperty("bluetoothManager", &bluetoothManager);
    engine.rootContext()->setContextProperty("contactManager", &contactManager);
    engine.rootContext()->setContextProperty("messageManager", &messageManager);
    engine.rootContext()->setContextProperty("voiceCommandHandler", &voiceCommandHandler);
    engine.rootContext()->setContextProperty("weatherManager", &weatherManager);
    engine.rootContext()->setContextProperty("vehicleBusManager", &vehicleBusManager);
    engine.rootContext()->setContextProperty("tidalClient", &tidalClient);
    engine.rootContext()->setContextProperty("spotifyClient", &spotifyClient);
    engine.rootContext()->setContextProperty("updateManager", &updateManager);

    // Project root directory (for loading large assets like splash videos from filesystem)
    QString projectDir = QCoreApplication::applicationDirPath() + "/..";
    engine.rootContext()->setContextProperty("projectDir", QUrl::fromLocalFile(QDir(projectDir).canonicalPath() + "/"));

    // Mapbox access token from environment
    QString mapboxToken = qEnvironmentVariable("MAPBOX_TOKEN", "");
    engine.rootContext()->setContextProperty("mapboxAccessToken", mapboxToken);
    if (mapboxToken.isEmpty()) {
        qWarning() << "MAPBOX_TOKEN not set - map will not load. Set it with: export MAPBOX_TOKEN=pk.your_token";
    }

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
    qDebug() << "  - Media:         " << &mediaController;
    qDebug() << "  - Voice:         " << &voiceAssistant;
    qDebug() << "  - Picovoice:     " << &picovoiceManager;
    qDebug() << "  - Claude AI:     " << &claudeClient;
    qDebug() << "  - Notifications: " << &notificationManager;
    qDebug() << "  - Bluetooth:     " << &bluetoothManager;
    qDebug() << "  - Contacts:      " << &contactManager;
    qDebug() << "  - Messages:      " << &messageManager;
    qDebug() << "  - VoiceCommands: " << &voiceCommandHandler;
    qDebug() << "  - VehicleBus:    " << &vehicleBusManager;
    qDebug() << "  - TidalClient:   " << &tidalClient;

    // Start unified voice pipeline (wake word + STT + noise suppression)
    qDebug() << "Starting Picovoice unified voice pipeline...";
    picovoiceManager.start();

    return app.exec();
}
