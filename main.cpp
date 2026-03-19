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
#include "ContextAggregator.h"
#include "PlacesSearchManager.h"
#include "RouteWeatherManager.h"
#include "CopilotMonitor.h"
#include "RoadConditionManager.h"
#include "SpeedLimitManager.h"
#include "RoadSurfaceManager.h"
#include "HighwayCameraManager.h"
#include "AvalancheManager.h"
#include "BorderWaitManager.h"
#include "ToolExecutor.h"
#include "AncsManager.h"

void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
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
    VoiceCommandHandler voiceCommandHandler;  // Kept for backward compat (unused by tool-use pipeline)
    WeatherManager weatherManager;
    VehicleBusManager vehicleBusManager;
    TidalClient tidalClient;
    tidalClient.connectToService();
    SpotifyClient spotifyClient;
    spotifyClient.connectToService();
    UpdateManager updateManager;

    // Wizard Copilot managers
    ContextAggregator contextAggregator;
    PlacesSearchManager placesSearchManager;
    RouteWeatherManager routeWeatherManager;
    CopilotMonitor copilotMonitor;
    RoadConditionManager roadConditionManager;
    SpeedLimitManager speedLimitManager;
    RoadSurfaceManager roadSurfaceManager;
    HighwayCameraManager highwayCameraManager;
    AvalancheManager avalancheManager;
    BorderWaitManager borderWaitManager;

    // ANCS — BLE peripheral advertising for iPhone notifications
    AncsManager ancsManager;

    // ToolExecutor — dispatches Claude tool calls to the right manager
    ToolExecutor toolExecutor;
    toolExecutor.setContactManager(&contactManager);
    toolExecutor.setMessageManager(&messageManager);
    toolExecutor.setBluetoothManager(&bluetoothManager);
    toolExecutor.setPlacesSearchManager(&placesSearchManager);
    toolExecutor.setTidalClient(&tidalClient);
    toolExecutor.setSpotifyClient(&spotifyClient);
    toolExecutor.setMediaController(&mediaController);
    toolExecutor.setCopilotMonitor(&copilotMonitor);

    // Wire ClaudeClient to use native tool calling via ToolExecutor
    claudeClient.setAvailableTools(ToolExecutor::toolDefinitions());
    claudeClient.setToolExecutor(&toolExecutor);

    // Set API keys from environment variables (loaded via .env)
    QString googleApiKey = qEnvironmentVariable("GOOGLE_API_KEY");
    QString picovoiceAccessKey = qEnvironmentVariable("PICOVOICE_ACCESS_KEY");
    googleTTS.setApiKey(googleApiKey);
    picovoiceManager.setAccessKey(picovoiceAccessKey);
    picovoiceManager.setGoogleApiKey(googleApiKey);
    if (googleApiKey.isEmpty()) {
        qWarning() << "GOOGLE_API_KEY not set - TTS and STT will not work. Set it with: export GOOGLE_API_KEY=your_key";
    }
    if (picovoiceAccessKey.isEmpty()) {
        qWarning() << "PICOVOICE_ACCESS_KEY not set - wake word and voice pipeline will not work. Set it with: export PICOVOICE_ACCESS_KEY=your_key";
    }

    // Set default TTS preferences (can be changed in Settings)
    googleTTS.setVoiceName("en-US-Studio-O");
    googleTTS.setSpeakingRate(1.0);
    googleTTS.setPitch(0.0);

    // Wake word and voice pipeline wiring is handled by VoicePipeline.qml

    QQmlApplicationEngine engine;

    // Set up BluetoothManager dependencies
    bluetoothManager.setContactManager(&contactManager);

    // Cascade BT disconnect to MediaController and TelephonyManager
    QObject::connect(&bluetoothManager, &BluetoothManager::deviceDisconnected,
                     &mediaController, [pMedia = &mediaController](const QString &) {
                         pMedia->disconnect();
                     });
    QObject::connect(&bluetoothManager, &BluetoothManager::deviceDisconnected,
                     bluetoothManager.telephonyManager(), &TelephonyManager::handleBluetoothDisconnect);

    // Wire Wizard Copilot managers
    contextAggregator.setWeatherManager(&weatherManager);
    contextAggregator.setVehicleBusManager(&vehicleBusManager);
    contextAggregator.setTidalClient(&tidalClient);
    contextAggregator.setSpotifyClient(&spotifyClient);
    contextAggregator.setMediaController(&mediaController);
    contextAggregator.setBluetoothManager(&bluetoothManager);
    placesSearchManager.setContextAggregator(&contextAggregator);
    placesSearchManager.setMapboxToken(qEnvironmentVariable("MAPBOX_TOKEN", ""));
    placesSearchManager.setGoogleApiKey(qEnvironmentVariable("GOOGLE_API_KEY"));
    routeWeatherManager.setContextAggregator(&contextAggregator);
    roadConditionManager.setContextAggregator(&contextAggregator);
    speedLimitManager.setContextAggregator(&contextAggregator);
    roadSurfaceManager.setContextAggregator(&contextAggregator);
    avalancheManager.setContextAggregator(&contextAggregator);
    borderWaitManager.setContextAggregator(&contextAggregator);
    copilotMonitor.setContextAggregator(&contextAggregator);
    copilotMonitor.setVehicleBusManager(&vehicleBusManager);
    copilotMonitor.setRouteWeatherManager(&routeWeatherManager);
    copilotMonitor.setRoadConditionManager(&roadConditionManager);
    copilotMonitor.setSpeedLimitManager(&speedLimitManager);
    copilotMonitor.setRoadSurfaceManager(&roadSurfaceManager);
    copilotMonitor.setAvalancheManager(&avalancheManager);
    copilotMonitor.setBorderWaitManager(&borderWaitManager);

    // Connect PicovoiceManager signals to handlers
    // Transcription ready -> send to Claude with live context from ContextAggregator
    QObject::connect(&picovoiceManager, &PicovoiceManager::transcriptionReady,
                     &claudeClient, [pClaude = &claudeClient, pCtx = &contextAggregator](const QString &text) {
                         pClaude->sendMessage(text, pCtx->buildContext());
                     });

    // Provide Claude with contact list for intelligent name matching
    QObject::connect(&contactManager, &ContactManager::syncCompleted,
                     &claudeClient, [pClaude = &claudeClient, pContacts = &contactManager, pPico = &picovoiceManager](int /*count*/) {
                         QStringList names = pContacts->getAllContactNames();
                         pClaude->setContactNames(names);
                         pPico->setSpeechContextHints(names);
                     });

    // Set initial contacts if already loaded from cache
    QStringList cachedNames = contactManager.getAllContactNames();
    if (!cachedNames.isEmpty()) {
        claudeClient.setContactNames(cachedNames);
        picovoiceManager.setSpeechContextHints(cachedNames);
    }

    // Note: Places search, follow-up mode, and quiet mode are now handled
    // internally by the ToolExecutor + ClaudeClient tool-use loop.
    // VoicePipeline.qml connects to ToolExecutor signals for navigation,
    // follow-up, and confirmation dialogs.

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
    engine.rootContext()->setContextProperty("contextAggregator", &contextAggregator);
    engine.rootContext()->setContextProperty("placesSearchManager", &placesSearchManager);
    engine.rootContext()->setContextProperty("routeWeatherManager", &routeWeatherManager);
    engine.rootContext()->setContextProperty("copilotMonitor", &copilotMonitor);
    engine.rootContext()->setContextProperty("roadConditionManager", &roadConditionManager);
    engine.rootContext()->setContextProperty("speedLimitManager", &speedLimitManager);
    engine.rootContext()->setContextProperty("roadSurfaceManager", &roadSurfaceManager);
    engine.rootContext()->setContextProperty("highwayCameraManager", &highwayCameraManager);
    engine.rootContext()->setContextProperty("avalancheManager", &avalancheManager);
    engine.rootContext()->setContextProperty("borderWaitManager", &borderWaitManager);
    engine.rootContext()->setContextProperty("toolExecutor", &toolExecutor);
    engine.rootContext()->setContextProperty("ancsManager", &ancsManager);

    // Project root directory (for loading large assets like splash videos from filesystem)
    QString projectDir = QCoreApplication::applicationDirPath() + "/..";
    engine.rootContext()->setContextProperty("projectDir", QUrl::fromLocalFile(QDir(projectDir).canonicalPath() + "/"));

    // Mapbox access token from environment
    QString mapboxToken = qEnvironmentVariable("MAPBOX_TOKEN", "");
    engine.rootContext()->setContextProperty("mapboxAccessToken", mapboxToken);
    if (mapboxToken.isEmpty()) {
        qWarning() << "MAPBOX_TOKEN not set - map will not load. Set it with: export MAPBOX_TOKEN=pk.your_token";
    }

    // OpenWeatherMap API key for radar tiles
    QString owmApiKey = qEnvironmentVariable("OWM_API_KEY", "");
    engine.rootContext()->setContextProperty("owmApiKey", owmApiKey);
    if (owmApiKey.isEmpty()) {
        qWarning() << "OWM_API_KEY not set - radar overlay will use RainViewer fallback";
    }

    // Load QML - version compatible approach
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() {
                         qCritical() << "QML Engine Creation Failed";
                         QCoreApplication::exit(-1);
                     },
                     Qt::QueuedConnection);

    engine.loadFromModule("HeadUnit", "Main");
#else
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/HeadUnit/Main.qml")));
#endif

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "No root objects created";
        return -1;
    }

    qDebug() << "=== HeadUnit Started Successfully ===";
    qDebug() << "Controllers initialized:";
    qDebug() << "  - Claude AI:     " << &claudeClient;
    qDebug() << "  - ToolExecutor:  " << &toolExecutor;
    qDebug() << "  - Picovoice:     " << &picovoiceManager;
    qDebug() << "  - VehicleBus:    " << &vehicleBusManager;
    qDebug() << "  - TidalClient:   " << &tidalClient;

    // Wire ANCS notifications into the notification system
    QObject::connect(&ancsManager, &AncsManager::notificationReceived,
                     &notificationManager, [&notificationManager](const QVariantMap &n) {
                         // Forward ANCS notification to the existing notification system
                         emit notificationManager.notificationReceived(n);
                     });

    // Wire incoming SMS (MAP) into the notification system
    QObject::connect(&messageManager, &MessageManager::newMessageReceived,
                     &notificationManager, [&notificationManager](const QString &threadId, const QString &sender, const QString &body) {
                         Q_UNUSED(threadId)
                         QVariantMap n;
                         n["title"] = sender;
                         n["message"] = body;
                         n["category"] = 1;  // Social/Message
                         n["priority"] = 2;  // Normal
                         n["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                         notificationManager.addNotification(n);
                     });

    // When LE bond completes and BR/EDR profiles connect, set up media/voice services
    QObject::connect(&ancsManager, &AncsManager::deviceBondedOverLE,
                     &mediaController, [&mediaController](const QString &address) {
                         qDebug() << "LE bond triggered BR/EDR service setup for" << address;
                         mediaController.connectToDevice(address);
                     });

    // Start BLE ANCS advertising immediately (doesn't block)
    ancsManager.startAdvertising();

    // Start unified voice pipeline after event loop is running
    // (audio device init can block if PulseAudio is reconfiguring)
    QTimer::singleShot(3000, &picovoiceManager, [&picovoiceManager]() {
        qDebug() << "Starting Picovoice unified voice pipeline...";
        picovoiceManager.start();
    });

    return app.exec();
}
