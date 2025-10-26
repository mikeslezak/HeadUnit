#include "MediaController.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QNetworkRequest>
#include <QBuffer>
#include <QImageReader>

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
    , m_socket(nullptr)
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
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
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
 * Establishes AVRCP connection to phone's music service
 *
 * Real Mode: Connects via Bluetooth socket to AVRCP service UUID
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
    // Real mode - connect to AVRCP service
    setStatusMessage("Connecting to AVRCP service...");

    // AVRCP service UUID: 0x110E (AV Remote Control)
    static const QBluetoothUuid avrcpUuid(QBluetoothUuid::ServiceClassUuid::AV_RemoteControl);

    m_bluetoothAddress = QBluetoothAddress(deviceAddress);

    if (m_socket) {
        m_socket->deleteLater();
    }

    m_socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol, this);

    connect(m_socket, &QBluetoothSocket::connected,
            this, &MediaController::onSocketConnected);
    connect(m_socket, &QBluetoothSocket::disconnected,
            this, &MediaController::onSocketDisconnected);
    connect(m_socket, &QBluetoothSocket::errorOccurred,
            this, &MediaController::onSocketError);
    connect(m_socket, &QBluetoothSocket::readyRead,
            this, &MediaController::onSocketReadyRead);

    // Connect to AVRCP service
    m_socket->connectToService(m_bluetoothAddress, avrcpUuid);
#endif
}

/**
 * DISCONNECT
 *
 * Closes AVRCP connection
 */
void MediaController::disconnect()
{
#ifdef Q_OS_WIN
    m_isConnected = false;
    m_isPlaying = false;
    m_positionTimer->stop();
    emit connectionChanged();
    emit playStateChanged();
    setStatusMessage("Mock: Disconnected");
#else
    if (m_socket) {
        m_socket->close();
    }
    m_isConnected = false;
    m_isPlaying = false;
    m_positionTimer->stop();
    emit connectionChanged();
    emit playStateChanged();
    setStatusMessage("Disconnected");
#endif
}

// ========================================================================
// PLAYBACK CONTROLS
// ========================================================================

/**
 * PLAY
 *
 * Sends AVRCP Play command to phone
 * Command: 0x44 (PLAY)
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
    sendAvrcpCommand("PLAY");
#endif
}

/**
 * PAUSE
 *
 * Sends AVRCP Pause command to phone
 * Command: 0x46 (PAUSE)
 */
void MediaController::pause()
{
#ifdef Q_OS_WIN
    m_isPlaying = false;
    m_positionTimer->stop();
    m_mockTimer->stop();
    emit playStateChanged();
    setStatusMessage("Paused");
#else
    sendAvrcpCommand("PAUSE");
#endif
}

/**
 * STOP
 *
 * Sends AVRCP Stop command to phone
 * Command: 0x45 (STOP)
 *
 * Note: Stop resets position to 0, Pause keeps position
 */
void MediaController::stop()
{
#ifdef Q_OS_WIN
    m_isPlaying = false;
    m_trackPosition = 0;
    m_positionTimer->stop();
    m_mockTimer->stop();
    emit playStateChanged();
    emit positionChanged();
    setStatusMessage("Stopped");
#else
    sendAvrcpCommand("STOP");
#endif
}

/**
 * TOGGLE PLAY/PAUSE
 *
 * Convenience method - plays if paused, pauses if playing
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
 *
 * Sends AVRCP Next command to phone
 * Command: 0x4B (FORWARD)
 */
void MediaController::next()
{
#ifdef Q_OS_WIN
    simulateTrackChange();
    setStatusMessage("Next track");
#else
    sendAvrcpCommand("FORWARD");
#endif
}

/**
 * PREVIOUS TRACK
 *
 * Sends AVRCP Previous command to phone
 * Command: 0x4C (BACKWARD)
 *
 * Note: If >3 seconds into track, restarts current track
 *       If <3 seconds, goes to previous track
 */
void MediaController::previous()
{
#ifdef Q_OS_WIN
    if (m_trackPosition > 3000) {
        // Restart current track
        m_trackPosition = 0;
        emit positionChanged();
        setStatusMessage("Restarting track");
    } else {
        // Go to previous track
        simulateTrackChange();
        setStatusMessage("Previous track");
    }
#else
    sendAvrcpCommand("BACKWARD");
#endif
}

/**
 * SEEK TO POSITION
 *
 * Seeks to specific position in current track
 *
 * @param positionMs: Target position in milliseconds
 *
 * Note: Not all phones support seeking via AVRCP
 *       iPhone requires AVRCP 1.5+
 */
void MediaController::seekTo(qint64 positionMs)
{
    if (positionMs < 0 || positionMs > m_trackDuration) {
        qWarning() << "Invalid seek position:" << positionMs;
        return;
    }

#ifdef Q_OS_WIN
    m_trackPosition = positionMs;
    emit positionChanged();
    setStatusMessage("Seeked to " + QString::number(positionMs / 1000) + "s");
#else
    // Send AVRCP seek command (AVRCP 1.5+)
    sendAvrcpCommand("SEEK:" + QString::number(positionMs));
#endif
}

/**
 * SKIP FORWARD
 *
 * Jumps forward by specified seconds
 */
void MediaController::skipForward(int seconds)
{
    qint64 newPos = qMin(m_trackPosition + (seconds * 1000), m_trackDuration);
    seekTo(newPos);
}

/**
 * SKIP BACKWARD
 *
 * Jumps backward by specified seconds
 */
void MediaController::skipBackward(int seconds)
{
    qint64 newPos = qMax(m_trackPosition - (seconds * 1000), (qint64)0);
    seekTo(newPos);
}

// ========================================================================
// VOLUME CONTROL
// ========================================================================

/**
 * SET VOLUME
 *
 * Sets absolute volume level
 *
 * @param level: Volume 0-100
 *
 * Command: AVRCP Absolute Volume (0x50)
 */
void MediaController::setVolume(int level)
{
    level = qBound(0, level, 100);

#ifdef Q_OS_WIN
    m_volume = level;
    m_isMuted = false;
    emit volumeChanged();
    setStatusMessage("Volume: " + QString::number(level) + "%");
#else
    sendAvrcpCommand("VOLUME:" + QString::number(level));
    m_volume = level;
    emit volumeChanged();
#endif
}

/**
 * VOLUME UP
 *
 * Increases volume by step amount
 */
void MediaController::volumeUp(int step)
{
    setVolume(m_volume + step);
}

/**
 * VOLUME DOWN
 *
 * Decreases volume by step amount
 */
void MediaController::volumeDown(int step)
{
    setVolume(m_volume - step);
}

/**
 * TOGGLE MUTE
 *
 * Mutes by saving current volume and setting to 0
 * Unmutes by restoring saved volume
 */
void MediaController::toggleMute()
{
    if (m_isMuted) {
        // Unmute - restore saved volume
        setVolume(m_savedVolume);
        m_isMuted = false;
    } else {
        // Mute - save current and set to 0
        m_savedVolume = m_volume;
        setVolume(0);
        m_isMuted = true;
    }
}

// ========================================================================
// PLAYBACK MODES
// ========================================================================

/**
 * SET REPEAT MODE
 *
 * Changes repeat mode: Off, All, or One
 *
 * Note: Requires AVRCP 1.4+ support on phone
 */
void MediaController::setRepeatMode(RepeatMode mode)
{
#ifdef Q_OS_WIN
    m_repeatMode = mode;
    emit repeatModeChanged();

    QString modeStr = (mode == RepeatOff) ? "Off" :
                          (mode == RepeatAll) ? "All" : "One";
    setStatusMessage("Repeat: " + modeStr);
#else
    sendAvrcpCommand("REPEAT:" + QString::number((int)mode));
    m_repeatMode = mode;
    emit repeatModeChanged();
#endif
}

/**
 * CYCLE REPEAT MODE
 *
 * Cycles through: Off â†’ All â†’ One â†’ Off
 */
void MediaController::cycleRepeatMode()
{
    RepeatMode newMode;
    switch (m_repeatMode) {
    case RepeatOff:
        newMode = RepeatAll;
        break;
    case RepeatAll:
        newMode = RepeatOne;
        break;
    case RepeatOne:
        newMode = RepeatOff;
        break;
    }
    setRepeatMode(newMode);
}

/**
 * SET SHUFFLE
 *
 * Enables or disables shuffle mode
 */
void MediaController::setShuffle(bool enabled)
{
#ifdef Q_OS_WIN
    m_shuffleEnabled = enabled;
    emit shuffleChanged();
    setStatusMessage("Shuffle: " + QString(enabled ? "On" : "Off"));
#else
    sendAvrcpCommand("SHUFFLE:" + QString(enabled ? "1" : "0"));
    m_shuffleEnabled = enabled;
    emit shuffleChanged();
#endif
}

/**
 * TOGGLE SHUFFLE
 *
 * Toggles shuffle on/off
 */
void MediaController::toggleShuffle()
{
    setShuffle(!m_shuffleEnabled);
}

// ========================================================================
// AUDIO SOURCE SWITCHING
// ========================================================================

/**
 * SET AUDIO SOURCE
 *
 * Switches between different audio sources:
 * - "phone": Bluetooth A2DP from phone
 * - "tidal": Local Tidal app
 * - "radio": FM/AM radio
 * - "aux": Auxiliary input
 * - "usb": USB media
 */
void MediaController::setAudioSource(const QString &source)
{
    if (m_audioSource != source) {
        m_audioSource = source;
        emit audioSourceChanged();
        setStatusMessage("Audio source: " + source);

        // When switching away from phone, pause playback
        if (source != "phone" && m_isPlaying) {
            pause();
        }
    }
}

// ========================================================================
// LIBRARY BROWSING
// ========================================================================

/**
 * REQUEST PLAYLISTS
 *
 * Requests list of playlists from phone
 *
 * Note: Requires AVRCP 1.4+ with browsing support
 *       Not all phones support this feature
 */
void MediaController::requestPlaylists()
{
#ifdef Q_OS_WIN
    // Generate mock playlists
    QVariantList playlists;
    playlists.append(QVariantMap{{"id", "1"}, {"name", "Favorites"}, {"trackCount", 47}});
    playlists.append(QVariantMap{{"id", "2"}, {"name", "Road Trip"}, {"trackCount", 32}});
    playlists.append(QVariantMap{{"id", "3"}, {"name", "Workout"}, {"trackCount", 28}});

    emit playlistsReceived(playlists);
    setStatusMessage("Loaded " + QString::number(playlists.size()) + " playlists");
#else
    sendAvrcpCommand("GET_PLAYLISTS");
#endif
}

/**
 * REQUEST ARTISTS
 *
 * Requests list of artists from phone library
 */
void MediaController::requestArtists()
{
#ifdef Q_OS_WIN
    QVariantList artists;
    artists.append("The Weeknd");
    artists.append("Dua Lipa");
    artists.append("Drake");
    artists.append("Taylor Swift");

    emit artistsReceived(artists);
#else
    sendAvrcpCommand("GET_ARTISTS");
#endif
}

/**
 * REQUEST ALBUMS
 *
 * Requests list of albums from phone library
 */
void MediaController::requestAlbums()
{
#ifdef Q_OS_WIN
    QVariantList albums;
    albums.append(QVariantMap{{"title", "After Hours"}, {"artist", "The Weeknd"}});
    albums.append(QVariantMap{{"title", "Future Nostalgia"}, {"artist", "Dua Lipa"}});

    emit albumsReceived(albums);
#else
    sendAvrcpCommand("GET_ALBUMS");
#endif
}

/**
 * PLAY PLAYLIST
 *
 * Starts playing specific playlist by ID
 */
void MediaController::playPlaylist(const QString &playlistId)
{
#ifdef Q_OS_WIN
    qDebug() << "Mock: Playing playlist" << playlistId;
    simulateTrackChange();
    play();
#else
    sendAvrcpCommand("PLAY_PLAYLIST:" + playlistId);
#endif
}

// ========================================================================
// POSITION TRACKING
// ========================================================================

/**
 * UPDATE POSITION
 *
 * Called every second by m_positionTimer while playing
 * Increments track position and emits signal for UI updates
 */
void MediaController::updatePosition()
{
    if (m_isPlaying && m_trackPosition < m_trackDuration) {
        m_trackPosition += 1000;
        emit positionChanged();

        // Check if track ended
        if (m_trackPosition >= m_trackDuration) {
            // Handle repeat mode
            if (m_repeatMode == RepeatOne) {
                m_trackPosition = 0;
                emit positionChanged();
            } else {
                // Auto-advance to next track
                next();
            }
        }
    }
}

// ========================================================================
// ALBUM ART MANAGEMENT
// ========================================================================

/**
 * DOWNLOAD ALBUM ART
 *
 * Downloads album artwork from URL provided in metadata
 *
 * @param url: Image URL (usually from music service API)
 */
void MediaController::downloadAlbumArt(const QUrl &url)
{
    if (!url.isValid()) {
        qDebug() << "Invalid album art URL";
        return;
    }

    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished,
            this, &MediaController::onAlbumArtDownloaded);
}

/**
 * ON ALBUM ART DOWNLOADED
 *
 * Handles album art download completion
 * Loads image and emits signal for UI update
 */
void MediaController::onAlbumArtDownloaded()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::ReadOnly);

        QImageReader reader(&buffer);
        m_albumArtImage = reader.read();

        if (!m_albumArtImage.isNull()) {
            qDebug() << "Album art downloaded successfully";
            emit albumArtChanged();
        } else {
            qWarning() << "Failed to decode album art image";
        }
    } else {
        qWarning() << "Album art download failed:" << reply->errorString();
    }

    reply->deleteLater();
}

// ========================================================================
// BLUETOOTH COMMUNICATION (Real Mode Only)
// ========================================================================

#ifndef Q_OS_WIN

void MediaController::onSocketConnected()
{
    m_isConnected = true;
    emit connectionChanged();
    setStatusMessage("AVRCP connected");

    // Request initial track info
    sendAvrcpCommand("GET_TRACK_INFO");
}

void MediaController::onSocketDisconnected()
{
    m_isConnected = false;
    m_isPlaying = false;
    m_positionTimer->stop();
    emit connectionChanged();
    emit playStateChanged();
    setStatusMessage("AVRCP disconnected");
}

void MediaController::onSocketError()
{
    qWarning() << "AVRCP socket error:" << m_socket->errorString();
    emit error("Connection error: " + m_socket->errorString());
}

void MediaController::onSocketReadyRead()
{
    QByteArray data = m_socket->readAll();
    QString response = QString::fromUtf8(data);
    parseAvrcpResponse(response);
}

#endif

/**
 * SEND AVRCP COMMAND
 *
 * Sends command to phone via AVRCP protocol
 *
 * Real Mode: Sends actual AVRCP packets
 * Mock Mode: Just logs the command
 */
void MediaController::sendAvrcpCommand(const QString &command)
{
#ifdef Q_OS_WIN
    qDebug() << "Mock AVRCP command:" << command;
#else
    if (!m_socket || !m_socket->isOpen()) {
        qWarning() << "Cannot send command - socket not connected";
        return;
    }

    // Convert command to AVRCP packet format
    // This is simplified - real AVRCP uses binary protocol
    QByteArray packet = command.toUtf8() + "\r\n";
    m_socket->write(packet);

    qDebug() << "Sent AVRCP command:" << command;
#endif
}

/**
 * PARSE AVRCP RESPONSE
 *
 * Parses responses from phone containing metadata and state updates
 *
 * Expected formats:
 * - TRACK:title|artist|album|duration
 * - STATE:playing|paused|stopped
 * - POSITION:milliseconds
 * - VOLUME:level
 */
void MediaController::parseAvrcpResponse(const QString &response)
{
    qDebug() << "AVRCP response:" << response;

    if (response.startsWith("TRACK:")) {
        // Parse track metadata
        QStringList parts = response.mid(6).split("|");
        if (parts.size() >= 4) {
            m_trackTitle = parts[0];
            m_artist = parts[1];
            m_album = parts[2];
            m_trackDuration = parts[3].toLongLong();
            m_trackPosition = 0;

            emit trackChanged();
            emit durationChanged();
            emit positionChanged();
        }
    } else if (response.startsWith("STATE:")) {
        QString state = response.mid(6);
        bool wasPlaying = m_isPlaying;
        m_isPlaying = (state == "playing");

        if (m_isPlaying && !wasPlaying) {
            m_positionTimer->start();
        } else if (!m_isPlaying && wasPlaying) {
            m_positionTimer->stop();
        }

        emit playStateChanged();
    } else if (response.startsWith("POSITION:")) {
        m_trackPosition = response.mid(9).toLongLong();
        emit positionChanged();
    } else if (response.startsWith("VOLUME:")) {
        m_volume = response.mid(7).toInt();
        emit volumeChanged();
    }
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
