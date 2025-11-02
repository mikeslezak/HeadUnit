#include "MediaController.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QNetworkRequest>
#include <QBuffer>
#include <QImageReader>

// DBus includes for BlueZ integration
#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QProcess>
#endif

/**
 * CONSTRUCTOR
 *
 * Initializes the MediaController with default values.
 * Sets up timers for position tracking and mock data (if Windows).
 *
 * On Windows: Runs in mock mode with simulated music playback
 * On Linux/Embedded: Sets up real Bluetooth AVRCP connection
 */
MediaController::MediaController(QObject *parent)
    : QObject(parent)
    , m_isConnected(false)
    , m_deviceAddress("")
    , m_isPlaying(false)
    , m_repeatMode(RepeatOff)
    , m_shuffleEnabled(false)
    , m_trackTitle("No Track")
    , m_artist("Unknown Artist")
    , m_album("Unknown Album")
    , m_genre("")
    , m_trackPosition(0)
    , m_trackDuration(0)
    , m_volume(50)
    , m_savedVolume(50)
    , m_isMuted(false)
    , m_activeApp("")
    , m_audioSource("phone")
    , m_statusMessage("Ready")
    , m_positionTimer(new QTimer(this))
    , m_mockTimer(new QTimer(this))
    , m_networkManager(new QNetworkAccessManager(this))
#ifndef Q_OS_WIN
    , m_deviceInterface(nullptr)
    , m_mediaPlayerInterface(nullptr)
    , m_mediaControlInterface(nullptr)
    , m_mediaPlayerPath("")
    , m_pulseLoopbackModule(-1)
#endif
{
#ifdef Q_OS_WIN
    // ========== MOCK MODE (Windows Development) ==========
    m_mockMode = true;
    qDebug() << "MediaController: Running in MOCK mode (Windows)";
    setStatusMessage("Mock Mode - Simulated Bluetooth Music");

    // Simulate connection after 1 second
    QTimer::singleShot(1000, this, [this]() {
        m_isConnected = true;
        m_activeApp = "Spotify";
        emit connectionChanged();
        emit activeAppChanged();

        // Load initial mock track
        generateMockMusic();
        setStatusMessage("Mock: Connected to iPhone");
    });

    // Setup mock track changes every 3 minutes
    connect(m_mockTimer, &QTimer::timeout, this, &MediaController::simulateTrackChange);
    m_mockTimer->setInterval(180000); // 3 minutes

#else
    // ========== REAL MODE (Embedded Linux / Production) ==========
    m_mockMode = false;
    qDebug() << "MediaController: Real Bluetooth AVRCP mode";
    setStatusMessage("Ready to connect");
#endif

    // Setup position timer (updates every second while playing)
    connect(m_positionTimer, &QTimer::timeout, this, &MediaController::updatePosition);
    m_positionTimer->setInterval(1000);
}

/**
 * DESTRUCTOR
 *
 * Clean up resources
 */
MediaController::~MediaController()
{
#ifndef Q_OS_WIN
    // Clean up audio routing
    teardownAudioRouting();
    
    // Clean up DBus interfaces
    if (m_deviceInterface) {
        delete m_deviceInterface;
    }
    if (m_mediaPlayerInterface) {
        delete m_mediaPlayerInterface;
    }
    if (m_mediaControlInterface) {
        delete m_mediaControlInterface;
    }
#endif
}

/**
 * SET STATUS MESSAGE
 *
 * Updates the status message and emits signal for UI updates
 */
void MediaController::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "MediaController:" << msg;
    }
}

// ========================================================================
// CONNECTION MANAGEMENT
// ========================================================================

/**
 * CONNECT TO DEVICE
 *
 * Establishes AVRCP connection to phone's music service via BlueZ DBus
 *
 * Real Mode: Connects via DBus to BlueZ MediaPlayer1 interface
 * Mock Mode: Simulates connection
 */
void MediaController::connectToDevice(const QString &deviceAddress)
{
    m_deviceAddress = deviceAddress;

#ifdef Q_OS_WIN
    // Mock mode - simulate connection
    setStatusMessage("Mock: Connecting to music service...");

    QTimer::singleShot(1500, this, [this]() {
        m_isConnected = true;
        m_activeApp = "Apple Music";
        emit connectionChanged();
        emit activeAppChanged();

        generateMockMusic();
        setStatusMessage("Mock: Music control ready");
    });

#else
    // Real mode - connect via BlueZ DBus
    setStatusMessage("Connecting via BlueZ DBus...");
    
    qDebug() << "========================================";
    qDebug() << "MediaController: CONNECTING TO DEVICE";
    qDebug() << "MediaController: Device address:" << deviceAddress;
    qDebug() << "========================================";
    
    // Convert address format: XX:XX:XX:XX:XX:XX → dev_XX_XX_XX_XX_XX_XX
    QString devicePath = "/org/bluez/hci0/" + addressToPath(deviceAddress);
    
    qDebug() << "MediaController: Device DBus path:" << devicePath;
    
    // Create Device interface
    if (m_deviceInterface) {
        delete m_deviceInterface;
    }
    
    m_deviceInterface = new QDBusInterface(
        "org.bluez",
        devicePath,
        "org.bluez.Device1",
        QDBusConnection::systemBus(),
        this
    );
    
    if (!m_deviceInterface->isValid()) {
        QString errorMsg = "Failed to create Device interface: " + 
                          m_deviceInterface->lastError().message();
        qWarning() << "MediaController:" << errorMsg;
        emit error(errorMsg);
        setStatusMessage("Connection failed");
        return;
    }
    
    // Check if device is connected
    QVariant connected = m_deviceInterface->property("Connected");
    if (!connected.toBool()) {
        qWarning() << "MediaController: Device not connected at system level";
        emit error("Device not connected. Please pair and connect first.");
        setStatusMessage("Device not connected");
        return;
    }
    
    qDebug() << "MediaController: Device is connected, finding MediaPlayer...";
    
    // Find MediaPlayer object path
    m_mediaPlayerPath = findMediaPlayerPath(devicePath);
    
    if (m_mediaPlayerPath.isEmpty()) {
        qWarning() << "MediaController: No MediaPlayer found for device";
        emit error("No media player found. Start playing music on your phone.");
        setStatusMessage("No media player detected");
        return;
    }
    
    qDebug() << "MediaController: MediaPlayer path:" << m_mediaPlayerPath;
    
    // Create MediaPlayer interface
    if (m_mediaPlayerInterface) {
        delete m_mediaPlayerInterface;
    }
    
    m_mediaPlayerInterface = new QDBusInterface(
        "org.bluez",
        m_mediaPlayerPath,
        "org.bluez.MediaPlayer1",
        QDBusConnection::systemBus(),
        this
    );
    
    if (!m_mediaPlayerInterface->isValid()) {
        QString errorMsg = "Failed to create MediaPlayer interface: " + 
                          m_mediaPlayerInterface->lastError().message();
        qWarning() << "MediaController:" << errorMsg;
        emit error(errorMsg);
        setStatusMessage("MediaPlayer connection failed");
        return;
    }
    
    // Create MediaControl interface
    if (m_mediaControlInterface) {
        delete m_mediaControlInterface;
    }
    
    m_mediaControlInterface = new QDBusInterface(
        "org.bluez",
        devicePath,
        "org.bluez.MediaControl1",
        QDBusConnection::systemBus(),
        this
    );
    
    // Setup property monitoring
    setupPropertyMonitoring(m_mediaPlayerPath);
    
    // Monitor for new MediaPlayer objects (player switching)
    QDBusConnection::systemBus().connect(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "InterfacesAdded",
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap))
    );
    
    qDebug() << "MediaController: Property monitoring and InterfacesAdded listening set up";
    
    // Set up audio routing
    setupAudioRouting();
    
    // Read initial metadata using GetAll to bypass Qt's property cache issues
    qDebug() << "========================================";
    qDebug() << "MediaController: READING INITIAL METADATA";
    qDebug() << "========================================";
    
    // Call GetAll on org.freedesktop.DBus.Properties to get all properties at once
    QDBusMessage getAllMsg = QDBusMessage::createMethodCall(
        "org.bluez",
        m_mediaPlayerPath,
        "org.freedesktop.DBus.Properties",
        "GetAll"
    );
    getAllMsg << QString("org.bluez.MediaPlayer1");
    
    QDBusMessage getAllReply = QDBusConnection::systemBus().call(getAllMsg);
    
    QString status;
    QVariant nameVar;
    QVariantMap track;
    
    if (getAllReply.type() == QDBusMessage::ReplyMessage) {
        const QDBusArgument arg = getAllReply.arguments().at(0).value<QDBusArgument>();
        
        // Parse the returned properties dictionary
        arg.beginMap();
        while (!arg.atEnd()) {
            QString key;
            QVariant value;
            
            arg.beginMapEntry();
            arg >> key;
            
            QDBusVariant dbusVariant;
            arg >> dbusVariant;
            value = dbusVariant.variant();
            
            arg.endMapEntry();
            
            if (key == "Track") {
                track = extractTrackMetadata(value);
            } else if (key == "Status") {
                status = value.toString();
            } else if (key == "Name") {
                nameVar = value;
            }
        }
        arg.endMap();
        
        qDebug() << "MediaController: Successfully retrieved all properties via GetAll";
    } else {
        qWarning() << "MediaController: GetAll failed:" << getAllReply.errorMessage();
        // Fallback to individual property reads
        status = m_mediaPlayerInterface->property("Status").toString();
        nameVar = m_mediaPlayerInterface->property("Name");
        qDebug() << "MediaController: Using fallback property reads";
    }
    
    qDebug() << "MediaController: Initial Status:" << status;
    qDebug() << "MediaController: Initial Name property:" << nameVar 
             << "(valid:" << nameVar.isValid() << ")";
    qDebug() << "MediaController: Track metadata keys:" << track.keys();
    
    if (!track.isEmpty()) {
        m_trackTitle = track.value("Title", "Unknown Track").toString();
        m_artist = track.value("Artist", "Unknown Artist").toString();
        m_album = track.value("Album", "Unknown Album").toString();
        m_genre = track.value("Genre", "").toString();
        m_trackDuration = track.value("Duration", 0).toLongLong();
        m_trackPosition = m_mediaPlayerInterface->property("Position").toLongLong();
        
        qDebug() << "MediaController: Initial track:" << m_trackTitle;
        qDebug() << "MediaController: Artist:" << m_artist;
        qDebug() << "MediaController: Album:" << m_album;
        
        emit trackChanged();
        emit durationChanged();
        emit positionChanged();
        
        // Try to get album art URL from track metadata
        if (track.contains("AlbumArt")) {
            QUrl artUrl(track.value("AlbumArt").toString());
            qDebug() << "MediaController: Album art URL from metadata:" << artUrl;
            if (artUrl.isValid()) {
                downloadAlbumArt(artUrl);
            }
        } else {
            qDebug() << "MediaController: No album art URL found in track metadata";
        }
    }
    
    // Get app name
    QString appName;
    if (nameVar.isValid() && !nameVar.toString().isEmpty()) {
        appName = nameVar.toString();
        qDebug() << "MediaController: App name from Name property:" << appName;
    } else {
        // Fallback to device alias
        QVariant aliasVar = m_deviceInterface->property("Alias");
        appName = aliasVar.toString();
        qDebug() << "MediaController: Using device alias as app name:" << appName;
    }
    
    m_activeApp = appName;
    
    // Update play state
    m_isPlaying = (status == "playing");
    if (m_isPlaying) {
        m_positionTimer->start();
    }
    
    // Mark as connected
    m_isConnected = true;
    
    emit connectionChanged();
    emit playStateChanged();
    emit activeAppChanged();
    
    setStatusMessage("Connected to " + m_activeApp);
    
    qDebug() << "========================================";
    qDebug() << "MediaController: CONNECTION COMPLETE";
    qDebug() << "MediaController: Connected:" << m_isConnected;
    qDebug() << "MediaController: Playing:" << m_isPlaying;
    qDebug() << "MediaController: App:" << m_activeApp;
    qDebug() << "========================================";
#endif
}

/**
 * DISCONNECT
 *
 * Disconnect from device and clean up resources
 */
void MediaController::disconnect()
{
#ifdef Q_OS_WIN
    m_isConnected = false;
    m_isPlaying = false;
    m_positionTimer->stop();
    m_mockTimer->stop();
    
    emit connectionChanged();
    emit playStateChanged();
    
    setStatusMessage("Disconnected (Mock)");
#else
    qDebug() << "MediaController: Disconnecting...";
    
    // Stop timers
    m_positionTimer->stop();
    
    // Tear down audio routing
    teardownAudioRouting();
    
    // Disconnect from DBus signals
    QDBusConnection::systemBus().disconnect(
        "org.bluez",
        m_mediaPlayerPath,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))
    );
    
    QDBusConnection::systemBus().disconnect(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "InterfacesAdded",
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap))
    );
    
    // Clean up interfaces
    if (m_deviceInterface) {
        delete m_deviceInterface;
        m_deviceInterface = nullptr;
    }
    if (m_mediaPlayerInterface) {
        delete m_mediaPlayerInterface;
        m_mediaPlayerInterface = nullptr;
    }
    if (m_mediaControlInterface) {
        delete m_mediaControlInterface;
        m_mediaControlInterface = nullptr;
    }
    
    // Reset state
    m_isConnected = false;
    m_isPlaying = false;
    m_deviceAddress = "";
    m_mediaPlayerPath = "";
    m_activeApp = "";
    m_trackTitle = "No Track";
    m_artist = "Unknown Artist";
    m_album = "Unknown Album";
    
    emit connectionChanged();
    emit playStateChanged();
    emit trackChanged();
    emit activeAppChanged();
    
    setStatusMessage("Disconnected");
#endif
}

// ========================================================================
// PLAYBACK CONTROLS
// ========================================================================

/**
 * PLAY
 */
void MediaController::play()
{
#ifdef Q_OS_WIN
    m_isPlaying = true;
    m_positionTimer->start();
    m_mockTimer->start();
    emit playStateChanged();
    setStatusMessage("Playing: " + m_trackTitle);
#else
    if (!m_mediaPlayerInterface) {
        qWarning() << "MediaController: No media player interface";
        return;
    }
    
    qDebug() << "MediaController: Sending Play command";
    QDBusMessage response = m_mediaPlayerInterface->call("Play");
    
    if (response.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MediaController: Play failed:" << response.errorMessage();
        emit error("Play failed: " + response.errorMessage());
    } else {
        qDebug() << "MediaController: Play command sent successfully";
    }
#endif
}

/**
 * PAUSE
 */
void MediaController::pause()
{
#ifdef Q_OS_WIN
    m_isPlaying = false;
    m_positionTimer->stop();
    emit playStateChanged();
    setStatusMessage("Paused");
#else
    if (!m_mediaPlayerInterface) {
        qWarning() << "MediaController: No media player interface";
        return;
    }
    
    qDebug() << "MediaController: Sending Pause command";
    QDBusMessage response = m_mediaPlayerInterface->call("Pause");
    
    if (response.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MediaController: Pause failed:" << response.errorMessage();
        emit error("Pause failed: " + response.errorMessage());
    } else {
        qDebug() << "MediaController: Pause command sent successfully";
    }
#endif
}

/**
 * STOP
 */
void MediaController::stop()
{
#ifdef Q_OS_WIN
    pause();
    m_trackPosition = 0;
    emit positionChanged();
#else
    if (!m_mediaPlayerInterface) {
        return;
    }
    
    qDebug() << "MediaController: Sending Stop command";
    QDBusMessage response = m_mediaPlayerInterface->call("Stop");
    
    if (response.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MediaController: Stop failed:" << response.errorMessage();
    }
#endif
}

/**
 * TOGGLE PLAY/PAUSE
 */
void MediaController::togglePlayPause()
{
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

/**
 * NEXT TRACK
 */
void MediaController::next()
{
#ifdef Q_OS_WIN
    generateMockMusic();
    if (m_isPlaying) {
        setStatusMessage("Now playing: " + m_trackTitle);
    }
#else
    if (!m_mediaPlayerInterface) {
        qWarning() << "MediaController: No media player interface";
        return;
    }
    
    qDebug() << "MediaController: Sending Next command";
    QDBusMessage response = m_mediaPlayerInterface->call("Next");
    
    if (response.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MediaController: Next failed:" << response.errorMessage();
        emit error("Next failed: " + response.errorMessage());
    } else {
        qDebug() << "MediaController: Next command sent successfully";
    }
#endif
}

/**
 * PREVIOUS TRACK
 */
void MediaController::previous()
{
#ifdef Q_OS_WIN
    generateMockMusic();
    if (m_isPlaying) {
        setStatusMessage("Now playing: " + m_trackTitle);
    }
#else
    if (!m_mediaPlayerInterface) {
        qWarning() << "MediaController: No media player interface";
        return;
    }
    
    qDebug() << "MediaController: Sending Previous command";
    QDBusMessage response = m_mediaPlayerInterface->call("Previous");
    
    if (response.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MediaController: Previous failed:" << response.errorMessage();
        emit error("Previous failed: " + response.errorMessage());
    } else {
        qDebug() << "MediaController: Previous command sent successfully";
    }
#endif
}

/**
 * SEEK TO POSITION
 */
void MediaController::seekTo(qint64 positionMs)
{
#ifdef Q_OS_WIN
    m_trackPosition = positionMs;
    emit positionChanged();
#else
    // iOS typically doesn't support seeking via AVRCP
    qDebug() << "MediaController: Seek not supported by iOS AVRCP";
#endif
}

/**
 * SKIP FORWARD
 */
void MediaController::skipForward(int seconds)
{
    seekTo(m_trackPosition + (seconds * 1000));
}

/**
 * SKIP BACKWARD
 */
void MediaController::skipBackward(int seconds)
{
    seekTo(qMax(0LL, m_trackPosition - (seconds * 1000)));
}

// ========================================================================
// VOLUME CONTROL - INTENTIONALLY DISABLED
// ========================================================================

/**
 * These methods are disabled to maintain bit-perfect audio output.
 * Volume control is handled by the hardware amplifier.
 */

void MediaController::setVolume(int level)
{
    Q_UNUSED(level);
    qDebug() << "MediaController: Volume control disabled (bit-perfect audio)";
}

void MediaController::volumeUp(int step)
{
    Q_UNUSED(step);
    qDebug() << "MediaController: Volume control disabled (bit-perfect audio)";
}

void MediaController::volumeDown(int step)
{
    Q_UNUSED(step);
    qDebug() << "MediaController: Volume control disabled (bit-perfect audio)";
}

void MediaController::toggleMute()
{
    qDebug() << "MediaController: Mute control disabled (bit-perfect audio)";
}

// ========================================================================
// PLAYBACK MODES
// ========================================================================

void MediaController::setRepeatMode(RepeatMode mode)
{
    if (m_repeatMode != mode) {
        m_repeatMode = mode;
        emit repeatModeChanged();
        
#ifndef Q_OS_WIN
        // Try to set repeat mode via AVRCP (may not be supported by iOS)
        QString repeatValue;
        switch (mode) {
            case RepeatOff: repeatValue = "off"; break;
            case RepeatAll: repeatValue = "alltracks"; break;
            case RepeatOne: repeatValue = "singletrack"; break;
        }
        
        if (m_mediaPlayerInterface) {
            m_mediaPlayerInterface->setProperty("Repeat", repeatValue);
        }
#endif
    }
}

void MediaController::cycleRepeatMode()
{
    RepeatMode nextMode = static_cast<RepeatMode>((m_repeatMode + 1) % 3);
    setRepeatMode(nextMode);
}

void MediaController::setShuffle(bool enabled)
{
    if (m_shuffleEnabled != enabled) {
        m_shuffleEnabled = enabled;
        emit shuffleChanged();
        
#ifndef Q_OS_WIN
        // Try to set shuffle mode via AVRCP (may not be supported by iOS)
        if (m_mediaPlayerInterface) {
            m_mediaPlayerInterface->setProperty("Shuffle", enabled ? "alltracks" : "off");
        }
#endif
    }
}

void MediaController::toggleShuffle()
{
    setShuffle(!m_shuffleEnabled);
}

// ========================================================================
// AUDIO SOURCE SWITCHING
// ========================================================================

void MediaController::setAudioSource(const QString &source)
{
    if (m_audioSource != source) {
        m_audioSource = source;
        emit audioSourceChanged();
        setStatusMessage("Audio source: " + source);
    }
}

// ========================================================================
// LIBRARY BROWSING
// ========================================================================

void MediaController::requestPlaylists()
{
    qDebug() << "MediaController: Playlist browsing not implemented";
    // Would require Media Folder interface implementation
}

void MediaController::requestArtists()
{
    qDebug() << "MediaController: Artist browsing not implemented";
}

void MediaController::requestAlbums()
{
    qDebug() << "MediaController: Album browsing not implemented";
}

void MediaController::playPlaylist(const QString &playlistId)
{
    Q_UNUSED(playlistId);
    qDebug() << "MediaController: Playlist playback not implemented";
}

// ========================================================================
// INTERNAL SLOTS
// ========================================================================

/**
 * UPDATE POSITION
 *
 * Updates track position while playing
 */
void MediaController::updatePosition()
{
    if (m_isPlaying) {
        m_trackPosition += 1000; // Add 1 second in milliseconds
        
        // Wrap around at track end
        if (m_trackPosition >= m_trackDuration) {
            m_trackPosition = 0;
        }
        
        emit positionChanged();
    }
}

/**
 * DOWNLOAD ALBUM ART
 *
 * Downloads album art from URL
 */
void MediaController::downloadAlbumArt(const QUrl &url)
{
    qDebug() << "MediaController: Downloading album art from:" << url;
    
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, &MediaController::onAlbumArtDownloaded);
}

/**
 * ON ALBUM ART DOWNLOADED
 *
 * Handles downloaded album art
 */
void MediaController::onAlbumArtDownloaded()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        QImage image;
        
        if (image.loadFromData(imageData)) {
            m_albumArtImage = image;
            
            // Convert to data URL for QML
            QBuffer buffer;
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");
            QByteArray base64 = buffer.data().toBase64();
            m_albumArtUrl = QUrl("data:image/png;base64," + QString(base64));
            
            emit albumArtChanged();
            qDebug() << "MediaController: Album art loaded successfully";
        } else {
            qWarning() << "MediaController: Failed to load album art image";
        }
    } else {
        qWarning() << "MediaController: Album art download failed:" << reply->errorString();
    }
    
    reply->deleteLater();
}

// ========================================================================
// DBUS / BLUEZ INTEGRATION (Linux Only)
// ========================================================================

#ifndef Q_OS_WIN

/**
 * FIND MEDIA PLAYER PATH
 *
 * Finds the MediaPlayer object path for the given device
 */
QString MediaController::findMediaPlayerPath(const QString &devicePath)
{
    qDebug() << "MediaController: Searching for MediaPlayer under device path:" << devicePath;
    
    // Use ObjectManager to find all objects
    QDBusInterface manager(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        QDBusConnection::systemBus()
    );
    
    QDBusMessage response = manager.call("GetManagedObjects");
    
    if (response.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "MediaController: GetManagedObjects failed:" << response.errorMessage();
        return QString();
    }
    
    // Parse the response
    const QDBusArgument arg = response.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();
    
    while (!arg.atEnd()) {
        QString objectPath;
        QVariantMap interfaces;
        
        arg.beginMapEntry();
        arg >> objectPath >> interfaces;
        arg.endMapEntry();
        
        // Check if this object is a MediaPlayer under our device
        if (objectPath.startsWith(devicePath) && 
            objectPath.contains("player") &&
            interfaces.contains("org.bluez.MediaPlayer1")) {
            
            qDebug() << "MediaController: Found MediaPlayer at:" << objectPath;
            
            // Log all available properties for debugging
            QVariantMap playerProps = interfaces.value("org.bluez.MediaPlayer1").toMap();
            qDebug() << "MediaController: MediaPlayer properties:" << playerProps.keys();
            
            return objectPath;
        }
    }
    
    arg.endMap();
    
    qDebug() << "MediaController: No MediaPlayer found";
    return QString();
}

/**
 * SETUP PROPERTY MONITORING
 *
 * Sets up DBus signal monitoring for property changes
 */
void MediaController::setupPropertyMonitoring(const QString &path)
{
    qDebug() << "========================================";
    qDebug() << "MediaController: SETTING UP PROPERTY MONITORING";
    qDebug() << "MediaController: Monitoring path:" << path;
    qDebug() << "========================================";
    
    // Connect to PropertiesChanged signal
    bool connected = QDBusConnection::systemBus().connect(
        "org.bluez",
        path,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))
    );
    
    if (connected) {
        qDebug() << "MediaController: PropertiesChanged signal connected successfully";
    } else {
        qWarning() << "MediaController: Failed to connect PropertiesChanged signal";
    }
}

/**
 * ADDRESS TO PATH
 *
 * Converts Bluetooth address to DBus path format
 * XX:XX:XX:XX:XX:XX → dev_XX_XX_XX_XX_XX_XX
 */
QString MediaController::addressToPath(const QString &address)
{
    QString path = address;
    path.replace(":", "_");
    return "dev_" + path;
}

/**
 * ON PROPERTIES CHANGED
 *
 * Handles property changes from BlueZ MediaPlayer1 interface
 * This is the key method for detecting track changes, play state changes, and app switches
 */
void MediaController::onPropertiesChanged(const QString &interface,
                                         const QVariantMap &changedProperties,
                                         const QStringList &invalidatedProperties)
{
    // ========== ENHANCED DEBUGGING OUTPUT ==========
    qDebug() << "========================================";
    qDebug() << "MediaController: PROPERTIES CHANGED EVENT";
    qDebug() << "Interface:" << interface;
    qDebug() << "Number of changed properties:" << changedProperties.size();
    qDebug() << "Changed property keys:" << changedProperties.keys();
    qDebug() << "Invalidated properties:" << invalidatedProperties;
    
    // Log ALL changed properties in detail
    for (auto it = changedProperties.constBegin(); it != changedProperties.constEnd(); ++it) {
        qDebug() << "  Property:" << it.key() 
                 << "  Type:" << it.value().typeName()
                 << "  Value:" << it.value();
    }
    qDebug() << "========================================";
    
    // Only process MediaPlayer1 interface changes
    if (interface != "org.bluez.MediaPlayer1") {
        qDebug() << "MediaController: Ignoring interface:" << interface;
        return;
    }
    
    // Handle track metadata changes
    if (changedProperties.contains("Track")) {
        qDebug() << "========================================";
        qDebug() << "MediaController: TRACK METADATA CHANGED";
        
        QVariant trackVariant = changedProperties.value("Track");
        QVariantMap track = extractTrackMetadata(trackVariant);
        qDebug() << "MediaController: Track metadata keys:" << track.keys();
        
        // Log all metadata fields
        for (auto it = track.constBegin(); it != track.constEnd(); ++it) {
            qDebug() << "  Metadata:" << it.key() << "=" << it.value();
        }
        
        QString newTitle = track.value("Title", m_trackTitle).toString();
        QString newArtist = track.value("Artist", m_artist).toString();
        QString newAlbum = track.value("Album", m_album).toString();
        
        bool hasTrackChanged = (newTitle != m_trackTitle || 
                               newArtist != m_artist || 
                               newAlbum != m_album);
        
        if (hasTrackChanged) {
            qDebug() << "MediaController: Track actually changed!";
            qDebug() << "  Old:" << m_trackTitle << "-" << m_artist;
            qDebug() << "  New:" << newTitle << "-" << newArtist;
            
            m_trackTitle = newTitle;
            m_artist = newArtist;
            m_album = newAlbum;
            m_genre = track.value("Genre", "").toString();
            m_trackDuration = track.value("Duration", m_trackDuration).toLongLong();
            m_trackPosition = 0; // Reset position on track change
            
            emit trackChanged();
            emit durationChanged();
            emit positionChanged();
            
            setStatusMessage("Now playing: " + m_trackTitle);
            
            // Check for album art
            if (track.contains("AlbumArt")) {
                QUrl artUrl(track.value("AlbumArt").toString());
                qDebug() << "MediaController: Album art URL in track:" << artUrl;
                if (artUrl.isValid()) {
                    downloadAlbumArt(artUrl);
                }
            } else {
                qDebug() << "MediaController: No AlbumArt in track metadata";
            }
        } else {
            qDebug() << "MediaController: Track metadata updated but track is same";
        }
        
        qDebug() << "========================================";
    }
    
    // Handle play status changes
    if (changedProperties.contains("Status")) {
        QString status = changedProperties.value("Status").toString();
        qDebug() << "========================================";
        qDebug() << "MediaController: STATUS CHANGED:" << status;
        qDebug() << "========================================";
        
        bool wasPlaying = m_isPlaying;
        m_isPlaying = (status == "playing");
        
        if (m_isPlaying != wasPlaying) {
            if (m_isPlaying) {
                m_positionTimer->start();
                setStatusMessage("Playing: " + m_trackTitle);
            } else {
                m_positionTimer->stop();
                setStatusMessage("Paused");
            }
            emit playStateChanged();
        }
    }
    
    // Handle app name changes (THIS IS CRITICAL FOR APP SWITCHING)
    if (changedProperties.contains("Name")) {
        qDebug() << "========================================";
        qDebug() << "MediaController: NAME PROPERTY CHANGED!";
        
        QVariant nameVar = changedProperties.value("Name");
        qDebug() << "MediaController: Name variant type:" << nameVar.typeName();
        qDebug() << "MediaController: Name variant valid:" << nameVar.isValid();
        qDebug() << "MediaController: Name value:" << nameVar.toString();
        
        QString newAppName = nameVar.toString();
        
        if (newAppName.isEmpty()) {
            qDebug() << "MediaController: Name property is empty, trying device Alias";
            if (m_deviceInterface) {
                QVariant aliasVar = m_deviceInterface->property("Alias");
                newAppName = aliasVar.toString();
                qDebug() << "MediaController: Device Alias:" << newAppName;
            }
        }
        
        if (!newAppName.isEmpty() && newAppName != m_activeApp) {
            qDebug() << "MediaController: App switched from '" << m_activeApp << "' to '" << newAppName << "'";
            m_activeApp = newAppName;
            emit activeAppChanged();
            setStatusMessage("Switched to " + m_activeApp);
            
            // Force a metadata refresh when app switches
            qDebug() << "MediaController: Forcing metadata refresh after app switch";
            m_trackTitle = "";
            m_artist = "";
            m_album = "";
            emit trackChanged();
        } else {
            qDebug() << "MediaController: Name property changed but app is same or empty";
        }
        
        qDebug() << "========================================";
    }
    
    // Handle position changes
    if (changedProperties.contains("Position")) {
        m_trackPosition = changedProperties.value("Position").toLongLong();
        emit positionChanged();
        // Don't log this every time as it's noisy
    }
    
    // Log if any properties were invalidated
    if (!invalidatedProperties.isEmpty()) {
        qDebug() << "========================================";
        qDebug() << "MediaController: PROPERTIES INVALIDATED:" << invalidatedProperties;
        qDebug() << "========================================";
    }
}

/**
 * ON INTERFACES ADDED
 *
 * Detects when a new MediaPlayer appears (e.g., switching from Tidal to Apple Music)
 * This is called when iOS creates a NEW player object rather than reusing the existing one
 */
void MediaController::onInterfacesAdded(const QDBusObjectPath &path,
                                       const QVariantMap &interfaces)
{
    QString pathStr = path.path();
    
    qDebug() << "========================================";
    qDebug() << "MediaController: INTERFACES ADDED EVENT";
    qDebug() << "Path:" << pathStr;
    qDebug() << "Interfaces:" << interfaces.keys();
    qDebug() << "========================================";
    
    // Check if this is a MediaPlayer under our connected device
    if (!m_deviceAddress.isEmpty()) {
        QString devicePath = addressToPath(m_deviceAddress);
        
        qDebug() << "MediaController: Checking if path" << pathStr 
                 << "is under device" << devicePath;
        
        if (pathStr.contains(devicePath) && 
            pathStr.contains("player") && 
            interfaces.contains("org.bluez.MediaPlayer1")) {
            
            qDebug() << "========================================";
            qDebug() << "MediaController: NEW MEDIAPLAYER DETECTED!";
            qDebug() << "New path:" << pathStr;
            qDebug() << "Old path:" << m_mediaPlayerPath;
            
            // Log all properties of the new player
            QVariantMap playerProps = interfaces.value("org.bluez.MediaPlayer1").toMap();
            qDebug() << "MediaController: New MediaPlayer properties:" << playerProps.keys();
            for (auto it = playerProps.constBegin(); it != playerProps.constEnd(); ++it) {
                qDebug() << "  " << it.key() << "=" << it.value();
            }
            qDebug() << "========================================";
            
            // Only switch if it's actually a different player
            if (pathStr != m_mediaPlayerPath) {
                qDebug() << "MediaController: Switching from" << m_mediaPlayerPath << "to" << pathStr;
                
                // Disconnect old player monitoring
                if (!m_mediaPlayerPath.isEmpty()) {
                    QDBusConnection::systemBus().disconnect(
                        "org.bluez",
                        m_mediaPlayerPath,
                        "org.freedesktop.DBus.Properties",
                        "PropertiesChanged",
                        this,
                        SLOT(onPropertiesChanged(QString,QVariantMap,QStringList))
                    );
                    qDebug() << "MediaController: Disconnected old player monitoring";
                }
                
                // Update to new player
                m_mediaPlayerPath = pathStr;
                
                // Recreate MediaPlayer interface
                if (m_mediaPlayerInterface) {
                    delete m_mediaPlayerInterface;
                }
                
                m_mediaPlayerInterface = new QDBusInterface(
                    "org.bluez",
                    m_mediaPlayerPath,
                    "org.bluez.MediaPlayer1",
                    QDBusConnection::systemBus(),
                    this
                );
                
                if (!m_mediaPlayerInterface->isValid()) {
                    qWarning() << "MediaController: Failed to create interface for new player";
                    qWarning() << "Error:" << m_mediaPlayerInterface->lastError().message();
                    return;
                }
                
                qDebug() << "MediaController: New MediaPlayer interface created successfully";
                
                // Setup monitoring for new player
                setupPropertyMonitoring(m_mediaPlayerPath);
                
                // Read new player's properties using GetAll
                QDBusMessage getAllMsg = QDBusMessage::createMethodCall(
                    "org.bluez",
                    m_mediaPlayerPath,
                    "org.freedesktop.DBus.Properties",
                    "GetAll"
                );
                getAllMsg << QString("org.bluez.MediaPlayer1");
                
                QDBusMessage getAllReply = QDBusConnection::systemBus().call(getAllMsg);
                
                QString newAppName;
                QVariantMap track;
                QString status;
                
                if (getAllReply.type() == QDBusMessage::ReplyMessage) {
                    const QDBusArgument arg = getAllReply.arguments().at(0).value<QDBusArgument>();
                    
                    // Parse the returned properties dictionary
                    arg.beginMap();
                    while (!arg.atEnd()) {
                        QString key;
                        QVariant value;
                        
                        arg.beginMapEntry();
                        arg >> key;
                        
                        QDBusVariant dbusVariant;
                        arg >> dbusVariant;
                        value = dbusVariant.variant();
                        
                        arg.endMapEntry();
                        
                        if (key == "Name") {
                            newAppName = value.toString();
                        } else if (key == "Track") {
                            track = extractTrackMetadata(value);
                        } else if (key == "Status") {
                            status = value.toString();
                        }
                    }
                    arg.endMap();
                    
                    qDebug() << "MediaController: Successfully retrieved new player properties via GetAll";
                } else {
                    qWarning() << "MediaController: GetAll failed for new player:" << getAllReply.errorMessage();
                    // Fallback
                    QVariant nameVar = m_mediaPlayerInterface->property("Name");
                    newAppName = nameVar.toString();
                    status = m_mediaPlayerInterface->property("Status").toString();
                }
                
                qDebug() << "MediaController: New player Name property:" << newAppName;
                
                if (newAppName.isEmpty()) {
                    qDebug() << "MediaController: New player Name is empty, using Alias";
                    if (m_deviceInterface) {
                        QVariant aliasVar = m_deviceInterface->property("Alias");
                        newAppName = aliasVar.toString();
                        qDebug() << "MediaController: Device Alias:" << newAppName;
                    }
                }
                
                if (!newAppName.isEmpty() && newAppName != m_activeApp) {
                    m_activeApp = newAppName;
                    emit activeAppChanged();
                    setStatusMessage("Switched to " + m_activeApp);
                    qDebug() << "MediaController: App changed to:" << m_activeApp;
                }
                
                // Process track metadata
                if (!track.isEmpty()) {
                    m_trackTitle = track.value("Title", "Unknown Track").toString();
                    m_artist = track.value("Artist", "Unknown Artist").toString();
                    m_album = track.value("Album", "Unknown Album").toString();
                    m_genre = track.value("Genre", "").toString();
                    m_trackDuration = track.value("Duration", 0).toLongLong();
                    m_trackPosition = 0;
                    
                    qDebug() << "MediaController: New track loaded:" << m_trackTitle << "-" << m_artist;
                    emit trackChanged();
                    emit durationChanged();
                    emit positionChanged();
                } else {
                    qDebug() << "MediaController: New player has no track data yet";
                }
                
                // Update play state
                bool wasPlaying = m_isPlaying;
                m_isPlaying = (status == "playing");
                
                qDebug() << "MediaController: New player status:" << status;
                
                if (m_isPlaying != wasPlaying) {
                    emit playStateChanged();
                    if (m_isPlaying) {
                        m_positionTimer->start();
                    } else {
                        m_positionTimer->stop();
                    }
                }
                
                qDebug() << "========================================";
                qDebug() << "MediaController: PLAYER SWITCH COMPLETE";
                qDebug() << "New app:" << m_activeApp;
                qDebug() << "New track:" << m_trackTitle;
                qDebug() << "Playing:" << m_isPlaying;
                qDebug() << "========================================";
            } else {
                qDebug() << "MediaController: Same player path, ignoring";
            }
        } else {
            qDebug() << "MediaController: Not a MediaPlayer for our device, ignoring";
        }
    } else {
        qDebug() << "MediaController: No device connected, ignoring InterfacesAdded";
    }
}

/**
 * SETUP AUDIO ROUTING
 *
 * Creates PulseAudio loopback from Bluetooth A2DP source to output sink
 * This routes the bit-perfect digital audio from phone to the DAC/amplifier
 */
void MediaController::setupAudioRouting()
{
    qDebug() << "MediaController: Setting up audio routing...";
    
    // Build the Bluetooth source name
    // Format: bluez_source.XX_XX_XX_XX_XX_XX.a2dp_source
    QString btSource = "bluez_source." + m_deviceAddress;
    btSource.replace(":", "_");
    btSource += ".a2dp_source";
    
    qDebug() << "MediaController: Bluetooth source:" << btSource;
    
    // Target sink - typically the default ALSA output
    // You can customize this to your specific audio hardware
    QString targetSink = "alsa_output.platform-sound.analog-stereo";
    
    // Create PulseAudio loopback module
    QProcess pactl;
    QStringList args;
    args << "load-module" << "module-loopback"
         << QString("source=%1").arg(btSource)
         << QString("sink=%1").arg(targetSink)
         << "latency_msec=1";  // Low latency for automotive use
    
    pactl.start("pactl", args);
    pactl.waitForFinished(3000);
    
    QString output = pactl.readAllStandardOutput().trimmed();
    QString error = pactl.readAllStandardError().trimmed();
    
    if (pactl.exitCode() == 0 && !output.isEmpty()) {
        m_pulseLoopbackModule = output.toInt();
        qDebug() << "MediaController: Audio routing created, module ID:" << m_pulseLoopbackModule;
        setStatusMessage("Audio routing active");
    } else {
        qWarning() << "MediaController: Failed to create audio routing:" << error;
        emit this->error("Audio routing failed: " + error);
        m_pulseLoopbackModule = -1;
    }
}

/**
 * TEARDOWN AUDIO ROUTING
 *
 * Removes PulseAudio loopback module when disconnecting
 */
void MediaController::teardownAudioRouting()
{
    if (m_pulseLoopbackModule < 0) {
        return;  // No module to tear down
    }
    
    qDebug() << "MediaController: Tearing down audio routing, module ID:" << m_pulseLoopbackModule;
    
    QProcess pactl;
    QStringList args;
    args << "unload-module" << QString::number(m_pulseLoopbackModule);
    
    pactl.start("pactl", args);
    pactl.waitForFinished(3000);
    
    if (pactl.exitCode() == 0) {
        qDebug() << "MediaController: Audio routing removed successfully";
        m_pulseLoopbackModule = -1;
    } else {
        qWarning() << "MediaController: Failed to remove audio routing:" 
                   << pactl.readAllStandardError();
    }
}

/**
 * EXTRACT TRACK METADATA
 *
 * Properly extracts track metadata from QDBusArgument
 * The Track property is a{sv} (dictionary of string to variant)
 * which requires manual deserialization from QDBusArgument
 */
QVariantMap MediaController::extractTrackMetadata(const QVariant &trackVariant)
{
    QVariantMap result;
    
    if (!trackVariant.isValid() || trackVariant.isNull()) {
        qDebug() << "MediaController: Track variant is invalid or null";
        return result;
    }
    
    // Check if this is a QDBusArgument
    if (trackVariant.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = trackVariant.value<QDBusArgument>();
        
        qDebug() << "MediaController: Deserializing QDBusArgument for Track";
        
        // The Track property is a dictionary: a{sv}
        arg.beginMap();
        while (!arg.atEnd()) {
            QString key;
            QVariant value;
            
            arg.beginMapEntry();
            arg >> key;
            
            // The value itself might be a variant, so extract it properly
            QDBusVariant dbusVariant;
            arg >> dbusVariant;
            value = dbusVariant.variant();
            
            arg.endMapEntry();
            
            result[key] = value;
            qDebug() << "  Track metadata:" << key << "=" << value;
        }
        arg.endMap();
        
        qDebug() << "MediaController: Extracted" << result.size() << "metadata fields";
    } else if (trackVariant.canConvert<QVariantMap>()) {
        // Sometimes it might already be a QVariantMap
        result = trackVariant.toMap();
        qDebug() << "MediaController: Track was already a QVariantMap with" << result.size() << "fields";
    } else {
        qWarning() << "MediaController: Track variant is neither QDBusArgument nor QVariantMap, type:" 
                   << trackVariant.typeName();
    }
    
    return result;
}

#endif

/**
 * SEND AVRCP COMMAND
 *
 * Legacy method - kept for compatibility but now uses DBus directly
 * Real implementations are in play(), pause(), next(), previous() methods
 */
void MediaController::sendAvrcpCommand(const QString &command)
{
#ifdef Q_OS_WIN
    qDebug() << "Mock AVRCP command:" << command;
#else
    qDebug() << "MediaController: sendAvrcpCommand called with:" << command;
    // Commands are now sent directly via DBus in individual methods
    // This function kept for compatibility
#endif
}

/**
 * PARSE AVRCP RESPONSE
 *
 * Legacy method - kept for compatibility
 * Metadata now comes via DBus PropertiesChanged signals
 */
void MediaController::parseAvrcpResponse(const QString &response)
{
    qDebug() << "MediaController: parseAvrcpResponse called (legacy):" << response;
    // Response parsing now handled by onPropertiesChanged in DBus mode
}

// ========================================================================
// MOCK DATA GENERATION (Testing Only)
// ========================================================================

/**
 * GENERATE MOCK MUSIC
 *
 * Creates fake track data for Windows testing
 */
void MediaController::generateMockMusic()
{
    QStringList tracks = {
        "Blinding Lights|The Weeknd|After Hours|200000",
        "Levitating|Dua Lipa|Future Nostalgia|203000",
        "Starboy|The Weeknd|Starboy|230000",
        "Don't Start Now|Dua Lipa|Future Nostalgia|183000",
        "One Dance|Drake|Views|173000"
    };

    QString track = tracks.at(QRandomGenerator::global()->bounded(tracks.size()));
    QStringList parts = track.split("|");

    m_trackTitle = parts[0];
    m_artist = parts[1];
    m_album = parts[2];
    m_trackDuration = parts[3].toLongLong();
    m_trackPosition = 0;

    // Generate mock album art URL
    m_albumArtUrl = QUrl("https://via.placeholder.com/300x300/00f0ff/0a0a0f?text=" + m_trackTitle);

    emit trackChanged();
    emit durationChanged();
    emit positionChanged();
    emit albumArtChanged();

    qDebug() << "Mock track loaded:" << m_trackTitle << "by" << m_artist;
}

/**
 * SIMULATE TRACK CHANGE
 *
 * Loads a random mock track (for testing)
 */
void MediaController::simulateTrackChange()
{
    generateMockMusic();

    if (m_isPlaying) {
        setStatusMessage("Now playing: " + m_trackTitle);
    }
}
