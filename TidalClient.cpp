#include "TidalClient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QRandomGenerator>

TidalClient::TidalClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_serviceProcess(nullptr)
    , m_reconnectTimer(new QTimer(this))
    , m_reconnectAttempts(0)
    , m_authCheckTimer(new QTimer(this))
    , m_positionTimer(new QTimer(this))
    , m_isConnected(false)
    , m_isLoggedIn(false)
    , m_isLoading(false)
    , m_isPlaying(false)
    , m_trackDuration(0)
    , m_position(0)
    , m_duration(0)
    , m_queuePosition(-1)
    , m_shuffleEnabled(false)
    , m_repeatMode(0)
    , m_pipeline(nullptr)
    , m_busWatchId(0)
    , m_pendingTrackId(-1)
{
    qDebug() << "TidalClient: Initializing...";

    // Socket signals
    connect(m_socket, &QLocalSocket::connected,
            this, &TidalClient::onSocketConnected);
    connect(m_socket, &QLocalSocket::disconnected,
            this, &TidalClient::onSocketDisconnected);
    connect(m_socket, &QLocalSocket::errorOccurred,
            this, &TidalClient::onSocketError);
    connect(m_socket, &QLocalSocket::readyRead,
            this, &TidalClient::onSocketReadyRead);

    // Reconnect timer
    m_reconnectTimer->setInterval(RECONNECT_INTERVAL_MS);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &TidalClient::onReconnectTimer);

    // Auth check polling timer
    m_authCheckTimer->setInterval(AUTH_CHECK_INTERVAL_MS);
    connect(m_authCheckTimer, &QTimer::timeout, this, [this]() {
        checkLogin();
    });

    // Position polling timer
    m_positionTimer->setInterval(POSITION_UPDATE_MS);
    connect(m_positionTimer, &QTimer::timeout,
            this, &TidalClient::onPositionTimer);

    // Initialize GStreamer
    initGStreamer();

    setStatusMessage("Initializing...");
    qDebug() << "TidalClient: Initialization complete";
}

TidalClient::~TidalClient()
{
    m_authCheckTimer->stop();
    m_reconnectTimer->stop();
    m_positionTimer->stop();

    destroyGStreamer();

    if (m_socket->state() == QLocalSocket::ConnectedState) {
        m_socket->disconnectFromServer();
    }

    if (m_serviceProcess) {
        m_serviceProcess->terminate();
        m_serviceProcess->waitForFinished(3000);
    }
}

// ========================================================================
// GSTREAMER
// ========================================================================

void TidalClient::initGStreamer()
{
    // gst_init is safe to call multiple times
    gst_init(nullptr, nullptr);

    m_pipeline = gst_element_factory_make("playbin", "tidal-playbin");
    if (!m_pipeline) {
        qCritical() << "TidalClient: Failed to create GStreamer playbin";
        return;
    }

    // Set up bus watch for EOS, errors, state changes
    GstBus *bus = gst_element_get_bus(m_pipeline);
    m_busWatchId = gst_bus_add_watch(bus, onBusMessage, this);
    gst_object_unref(bus);

    qDebug() << "TidalClient: GStreamer playbin initialized";
}

void TidalClient::destroyGStreamer()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        if (m_busWatchId > 0) {
            g_source_remove(m_busWatchId);
            m_busWatchId = 0;
        }
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}

void TidalClient::playUrl(const QString &url)
{
    if (!m_pipeline) {
        qWarning() << "TidalClient: No GStreamer pipeline";
        return;
    }

    qDebug() << "TidalClient: Playing URL:" << url.left(80) << "...";

    // Stop current playback
    gst_element_set_state(m_pipeline, GST_STATE_NULL);

    // Set new URI
    QByteArray urlBytes = url.toUtf8();
    g_object_set(m_pipeline, "uri", urlBytes.constData(), nullptr);

    // Start playback
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "TidalClient: Failed to start playback";
        setStatusMessage("Playback failed");
        emit error("Failed to start audio playback");
        return;
    }

    m_isPlaying = true;
    emit playStateChanged();
    m_positionTimer->start();
}

gboolean TidalClient::onBusMessage(GstBus *bus, GstMessage *msg, gpointer data)
{
    Q_UNUSED(bus)
    TidalClient *self = static_cast<TidalClient*>(data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        qDebug() << "TidalClient: End of stream";
        // Auto-advance to next track
        QMetaObject::invokeMethod(self, "next", Qt::QueuedConnection);
        break;

    case GST_MESSAGE_ERROR: {
        GError *err = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(msg, &err, &debug);
        qWarning() << "TidalClient: GStreamer error:" << err->message;
        if (debug) {
            qDebug() << "  Debug:" << debug;
            g_free(debug);
        }
        g_error_free(err);

        self->m_isPlaying = false;
        emit self->playStateChanged();
        self->m_positionTimer->stop();
        self->setStatusMessage("Playback error");
        break;
    }

    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->m_pipeline)) {
            GstState oldState, newState, pending;
            gst_message_parse_state_changed(msg, &oldState, &newState, &pending);

            bool wasPlaying = self->m_isPlaying;
            self->m_isPlaying = (newState == GST_STATE_PLAYING);

            if (wasPlaying != self->m_isPlaying) {
                emit self->playStateChanged();
                if (self->m_isPlaying) {
                    self->m_positionTimer->start();
                } else {
                    self->m_positionTimer->stop();
                }
            }

            // Get duration when we start playing
            if (newState == GST_STATE_PLAYING) {
                gint64 dur = 0;
                if (gst_element_query_duration(self->m_pipeline, GST_FORMAT_TIME, &dur)) {
                    qint64 durMs = dur / GST_MSECOND;
                    if (durMs != self->m_duration) {
                        self->m_duration = durMs;
                        emit self->durationChanged();
                    }
                }
            }
        }
        break;
    }

    default:
        break;
    }

    return TRUE;
}

void TidalClient::onPositionTimer()
{
    if (!m_pipeline || !m_isPlaying) return;

    gint64 pos = 0;
    if (gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &pos)) {
        qint64 posMs = pos / GST_MSECOND;
        if (posMs != m_position) {
            m_position = posMs;
            emit positionChanged();
        }
    }

    // Also re-query duration in case it wasn't available initially
    if (m_duration == 0) {
        gint64 dur = 0;
        if (gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &dur)) {
            qint64 durMs = dur / GST_MSECOND;
            if (durMs > 0 && durMs != m_duration) {
                m_duration = durMs;
                emit durationChanged();
            }
        }
    }
}

// ========================================================================
// SERVICE MANAGEMENT
// ========================================================================

void TidalClient::startService()
{
    if (m_serviceProcess) {
        if (m_serviceProcess->state() == QProcess::Running) {
            qDebug() << "TidalClient: Service already running";
            return;
        }
        m_serviceProcess->deleteLater();
    }

    m_serviceProcess = new QProcess(this);
    connect(m_serviceProcess, &QProcess::started,
            this, &TidalClient::onServiceStarted);
    connect(m_serviceProcess, &QProcess::errorOccurred,
            this, &TidalClient::onServiceError);

    connect(m_serviceProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray out = m_serviceProcess->readAllStandardOutput();
        qDebug() << "TidalService:" << out.trimmed();
    });
    connect(m_serviceProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray err = m_serviceProcess->readAllStandardError();
        qDebug() << "TidalService(err):" << err.trimmed();
    });

    QString projectDir = QCoreApplication::applicationDirPath() + "/..";
    QString scriptPath = QDir(projectDir).absoluteFilePath("services/tidal_service.py");

    if (!QFile::exists(scriptPath)) {
        qWarning() << "TidalClient: Service script not found:" << scriptPath;
        setStatusMessage("Service script not found");
        emit error("Tidal service script not found: " + scriptPath);
        return;
    }

    qDebug() << "TidalClient: Starting service:" << scriptPath;
    setStatusMessage("Starting Tidal service...");
    m_serviceProcess->start("python3", {scriptPath});
}

void TidalClient::connectToService()
{
    if (m_socket->state() == QLocalSocket::ConnectedState) {
        return;
    }

    qDebug() << "TidalClient: Connecting to" << SOCKET_PATH;
    setStatusMessage("Connecting...");
    m_socket->connectToServer(SOCKET_PATH);
}

void TidalClient::disconnectFromService()
{
    m_reconnectTimer->stop();
    m_authCheckTimer->stop();
    m_reconnectAttempts = 0;

    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        m_socket->disconnectFromServer();
    }
}

// ========================================================================
// AUTHENTICATION
// ========================================================================

void TidalClient::checkAuthStatus()
{
    QJsonObject cmd;
    cmd["cmd"] = "auth_status";
    sendCommand(cmd);
}

void TidalClient::startLogin()
{
    setLoading(true);
    setStatusMessage("Starting login flow...");

    QJsonObject cmd;
    cmd["cmd"] = "auth_login";
    sendCommand(cmd);
}

void TidalClient::checkLogin()
{
    QJsonObject cmd;
    cmd["cmd"] = "auth_check";
    sendCommand(cmd);
}

// ========================================================================
// SEARCH
// ========================================================================

void TidalClient::search(const QString &query, const QString &type, int limit)
{
    if (query.trimmed().isEmpty()) return;

    setLoading(true);
    setStatusMessage("Searching...");

    QJsonObject cmd;
    cmd["cmd"] = "search";
    cmd["query"] = query;
    cmd["type"] = type;
    cmd["limit"] = limit;
    sendCommand(cmd);
}

// ========================================================================
// PLAYBACK
// ========================================================================

void TidalClient::playTrack(int trackId)
{
    m_pendingTrackId = trackId;
    setLoading(true);
    setStatusMessage("Getting stream...");

    QJsonObject cmd;
    cmd["cmd"] = "play_track";
    cmd["track_id"] = trackId;
    sendCommand(cmd);
}

void TidalClient::playTrackInContext(int trackId, const QVariantList &trackList, int index)
{
    // Set the queue to the provided track list
    m_queue = trackList;
    m_queuePosition = index;

    // Generate shuffle order if shuffle is on
    if (m_shuffleEnabled) {
        m_shuffleOrder.clear();
        for (int i = 0; i < m_queue.size(); i++) {
            m_shuffleOrder.append(i);
        }
        // Fisher-Yates shuffle, but keep current track at position 0
        if (m_queuePosition >= 0 && m_queuePosition < m_shuffleOrder.size()) {
            m_shuffleOrder.swapItemsAt(0, m_queuePosition);
            for (int i = m_shuffleOrder.size() - 1; i > 1; i--) {
                int j = 1 + QRandomGenerator::global()->bounded(i);
                m_shuffleOrder.swapItemsAt(i, j);
            }
        }
    }

    emit queueChanged();
    emit queuePositionChanged();

    // Set metadata from queue item and request stream
    setTrackFromQueue(index);
    requestStreamForQueueItem(index);
}

void TidalClient::play()
{
    if (!m_pipeline) return;

    if (m_streamUrl.isEmpty()) {
        // Nothing loaded — try to play current queue item
        if (m_queuePosition >= 0 && m_queuePosition < m_queue.size()) {
            requestStreamForQueueItem(m_queuePosition);
        }
        return;
    }

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

void TidalClient::pause()
{
    if (!m_pipeline) return;
    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
}

void TidalClient::resume()
{
    play();
}

void TidalClient::stop()
{
    if (!m_pipeline) return;
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    m_isPlaying = false;
    m_position = 0;
    m_positionTimer->stop();
    emit playStateChanged();
    emit positionChanged();
}

void TidalClient::next()
{
    if (m_queue.isEmpty()) return;

    // Repeat one: replay current track
    if (m_repeatMode == 2) {
        requestStreamForQueueItem(m_queuePosition);
        return;
    }

    int nextPos;
    if (m_shuffleEnabled && !m_shuffleOrder.isEmpty()) {
        // Find current position in shuffle order
        int shuffleIdx = m_shuffleOrder.indexOf(m_queuePosition);
        shuffleIdx++;
        if (shuffleIdx >= m_shuffleOrder.size()) {
            if (m_repeatMode == 1) {
                shuffleIdx = 0; // Repeat all: wrap
            } else {
                stop();
                return;
            }
        }
        nextPos = m_shuffleOrder[shuffleIdx];
    } else {
        nextPos = m_queuePosition + 1;
        if (nextPos >= m_queue.size()) {
            if (m_repeatMode == 1) {
                nextPos = 0; // Repeat all: wrap
            } else {
                stop();
                return;
            }
        }
    }

    m_queuePosition = nextPos;
    emit queuePositionChanged();
    setTrackFromQueue(nextPos);
    requestStreamForQueueItem(nextPos);
}

void TidalClient::previous()
{
    if (m_queue.isEmpty()) return;

    // If more than 3 seconds in, restart current track
    if (m_position > 3000) {
        seekTo(0);
        return;
    }

    int prevPos;
    if (m_shuffleEnabled && !m_shuffleOrder.isEmpty()) {
        int shuffleIdx = m_shuffleOrder.indexOf(m_queuePosition);
        shuffleIdx--;
        if (shuffleIdx < 0) {
            if (m_repeatMode == 1) {
                shuffleIdx = m_shuffleOrder.size() - 1;
            } else {
                seekTo(0);
                return;
            }
        }
        prevPos = m_shuffleOrder[shuffleIdx];
    } else {
        prevPos = m_queuePosition - 1;
        if (prevPos < 0) {
            if (m_repeatMode == 1) {
                prevPos = m_queue.size() - 1;
            } else {
                seekTo(0);
                return;
            }
        }
    }

    m_queuePosition = prevPos;
    emit queuePositionChanged();
    setTrackFromQueue(prevPos);
    requestStreamForQueueItem(prevPos);
}

void TidalClient::seekTo(qint64 positionMs)
{
    if (!m_pipeline) return;

    gint64 pos = positionMs * GST_MSECOND;
    gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                            pos);
    m_position = positionMs;
    emit positionChanged();
}

void TidalClient::toggleShuffle()
{
    m_shuffleEnabled = !m_shuffleEnabled;

    if (m_shuffleEnabled && !m_queue.isEmpty()) {
        // Build shuffle order starting from current track
        m_shuffleOrder.clear();
        for (int i = 0; i < m_queue.size(); i++) {
            m_shuffleOrder.append(i);
        }
        if (m_queuePosition >= 0 && m_queuePosition < m_shuffleOrder.size()) {
            m_shuffleOrder.swapItemsAt(0, m_queuePosition);
            for (int i = m_shuffleOrder.size() - 1; i > 1; i--) {
                int j = 1 + QRandomGenerator::global()->bounded(i);
                m_shuffleOrder.swapItemsAt(i, j);
            }
        }
    } else {
        m_shuffleOrder.clear();
    }

    emit shuffleChanged();
}

void TidalClient::cycleRepeatMode()
{
    m_repeatMode = (m_repeatMode + 1) % 3;
    emit repeatModeChanged();
}

// ========================================================================
// QUEUE
// ========================================================================

void TidalClient::addToQueue(const QVariantList &tracks)
{
    m_queue.append(tracks);
    emit queueChanged();
}

void TidalClient::clearQueue()
{
    m_queue.clear();
    m_queuePosition = -1;
    m_shuffleOrder.clear();
    emit queueChanged();
    emit queuePositionChanged();
}

void TidalClient::removeFromQueue(int index)
{
    if (index < 0 || index >= m_queue.size()) return;

    m_queue.removeAt(index);

    // Adjust queue position
    if (index < m_queuePosition) {
        m_queuePosition--;
        emit queuePositionChanged();
    } else if (index == m_queuePosition) {
        // Removed current track — stop
        stop();
    }

    emit queueChanged();
}

// ========================================================================
// QUEUE HELPERS
// ========================================================================

void TidalClient::setTrackFromQueue(int index)
{
    if (index < 0 || index >= m_queue.size()) return;

    QVariantMap track = m_queue[index].toMap();
    m_trackTitle = track.value("title").toString();
    m_artist = track.value("artist").toString();
    m_album = track.value("album").toString();
    m_albumArtUrl = track.value("image_url").toString();
    m_trackDuration = track.value("duration").toInt();
    m_audioQuality = track.value("audio_quality").toString();
    m_position = 0;
    m_duration = m_trackDuration * 1000; // seconds to ms

    emit trackChanged();
    emit positionChanged();
    emit durationChanged();
}

void TidalClient::requestStreamForQueueItem(int index)
{
    if (index < 0 || index >= m_queue.size()) return;

    QVariantMap track = m_queue[index].toMap();
    int trackId = track.value("id").toInt();

    if (trackId <= 0) {
        qWarning() << "TidalClient: Invalid track ID in queue at position" << index;
        return;
    }

    playTrack(trackId);
}

// ========================================================================
// BROWSING
// ========================================================================

void TidalClient::getAlbum(int albumId)
{
    setLoading(true);
    QJsonObject cmd;
    cmd["cmd"] = "get_album";
    cmd["album_id"] = albumId;
    sendCommand(cmd);
}

void TidalClient::getArtist(int artistId)
{
    setLoading(true);
    QJsonObject cmd;
    cmd["cmd"] = "get_artist";
    cmd["artist_id"] = artistId;
    sendCommand(cmd);
}

void TidalClient::getPlaylist(const QString &playlistId)
{
    setLoading(true);
    QJsonObject cmd;
    cmd["cmd"] = "get_playlist";
    cmd["playlist_id"] = playlistId;
    sendCommand(cmd);
}

void TidalClient::getFavorites()
{
    setLoading(true);
    setStatusMessage("Loading favorites...");
    QJsonObject cmd;
    cmd["cmd"] = "favorites";
    sendCommand(cmd);
}

void TidalClient::getHome()
{
    setLoading(true);
    QJsonObject cmd;
    cmd["cmd"] = "home";
    sendCommand(cmd);
}

// ========================================================================
// FAVORITES
// ========================================================================

void TidalClient::addFavorite(int trackId)
{
    QJsonObject cmd;
    cmd["cmd"] = "add_favorite";
    cmd["track_id"] = trackId;
    sendCommand(cmd);
}

void TidalClient::removeFavorite(int trackId)
{
    QJsonObject cmd;
    cmd["cmd"] = "remove_favorite";
    cmd["track_id"] = trackId;
    sendCommand(cmd);
}

// ========================================================================
// SOCKET EVENTS
// ========================================================================

void TidalClient::onSocketConnected()
{
    qDebug() << "TidalClient: Connected to service";
    m_isConnected = true;
    m_reconnectAttempts = 0;
    m_reconnectTimer->stop();
    emit connectionChanged();
    setStatusMessage("Connected");
    checkAuthStatus();
}

void TidalClient::onSocketDisconnected()
{
    qDebug() << "TidalClient: Disconnected from service";
    m_isConnected = false;
    m_isLoggedIn = false;
    emit connectionChanged();
    emit authStatusChanged();
    setStatusMessage("Disconnected");

    if (m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        m_reconnectTimer->start();
    }
}

void TidalClient::onSocketError(QLocalSocket::LocalSocketError socketError)
{
    Q_UNUSED(socketError)

    if (m_isConnected) {
        qWarning() << "TidalClient: Socket error:" << m_socket->errorString();
    }

    m_isConnected = false;
    emit connectionChanged();

    if (m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        if (!m_reconnectTimer->isActive()) {
            m_reconnectTimer->start();
        }
    } else {
        setStatusMessage("Service unavailable");
    }
}

void TidalClient::onSocketReadyRead()
{
    m_readBuffer.append(m_socket->readAll());

    while (true) {
        int newlineIdx = m_readBuffer.indexOf('\n');
        if (newlineIdx < 0) break;

        QByteArray line = m_readBuffer.left(newlineIdx);
        m_readBuffer.remove(0, newlineIdx + 1);

        if (line.trimmed().isEmpty()) continue;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "TidalClient: JSON parse error:" << parseError.errorString();
            continue;
        }

        handleResponse(doc.object());
    }
}

void TidalClient::onReconnectTimer()
{
    m_reconnectAttempts++;

    if (m_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        m_reconnectTimer->stop();
        setStatusMessage("Service unavailable - check tidal_service.py");
        return;
    }

    connectToService();
}

void TidalClient::onServiceStarted()
{
    qDebug() << "TidalClient: Service process started";
    setStatusMessage("Service started, connecting...");
    QTimer::singleShot(1000, this, &TidalClient::connectToService);
}

void TidalClient::onServiceError(QProcess::ProcessError error)
{
    qWarning() << "TidalClient: Service process error:" << error;
    setStatusMessage("Failed to start Tidal service");
    emit this->error("Failed to start Tidal service process");
}

// ========================================================================
// PROTOCOL
// ========================================================================

void TidalClient::sendCommand(const QJsonObject &cmd)
{
    if (m_socket->state() != QLocalSocket::ConnectedState) {
        qWarning() << "TidalClient: Not connected, can't send:" << cmd["cmd"].toString();
        emit error("Not connected to Tidal service");
        return;
    }

    QJsonDocument doc(cmd);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
    m_socket->flush();
}

void TidalClient::handleResponse(const QJsonObject &response)
{
    QString cmd = response["cmd"].toString();
    bool ok = response["ok"].toBool();

    if (!ok) {
        QString errorMsg = response["error"].toString();
        qWarning() << "TidalClient: Command failed:" << cmd << errorMsg;
        setLoading(false);

        if (errorMsg == "Not logged in") {
            m_isLoggedIn = false;
            emit authStatusChanged();
            setStatusMessage("Not logged in");
        } else {
            setStatusMessage("Error: " + errorMsg);
            emit error(errorMsg);
        }
        return;
    }

    // ── auth_status ──
    if (cmd == "auth_status") {
        bool wasLoggedIn = m_isLoggedIn;
        m_isLoggedIn = response["logged_in"].toBool();
        m_userName = response["user"].toString();

        if (m_isLoggedIn != wasLoggedIn) {
            emit authStatusChanged();
        }

        if (m_isLoggedIn) {
            setStatusMessage("Logged in as " + m_userName);
        } else {
            setStatusMessage("Not logged in");
        }
    }

    // ── auth_login ──
    else if (cmd == "auth_login") {
        QJsonObject data = response["data"].toObject();
        m_loginUrl = data["verification_uri"].toString();
        m_loginCode = data["user_code"].toString();
        emit loginUrlChanged();
        setLoading(false);
        setStatusMessage("Visit URL and enter code: " + m_loginCode);
        m_authCheckTimer->start();
    }

    // ── auth_check ──
    else if (cmd == "auth_check") {
        bool loggedIn = response["logged_in"].toBool();
        if (loggedIn) {
            m_isLoggedIn = true;
            m_authCheckTimer->stop();
            m_loginUrl.clear();
            m_loginCode.clear();
            emit authStatusChanged();
            emit loginUrlChanged();
            setStatusMessage("Login successful!");
            checkAuthStatus();
        }
    }

    // ── search ──
    else if (cmd == "search") {
        QJsonArray dataArray = response["data"].toArray();
        m_searchResults.clear();
        for (const QJsonValue &val : dataArray) {
            m_searchResults.append(val.toObject().toVariantMap());
        }
        emit searchResultsChanged();
        setLoading(false);
        setStatusMessage(QString::number(m_searchResults.size()) + " results");
    }

    // ── play_track ──
    else if (cmd == "play_track") {
        QJsonObject data = response["data"].toObject();
        m_streamUrl = data["url"].toString();
        QString codec = data["codec"].toString();
        QString quality = data["quality"].toString();
        m_audioQuality = quality;

        emit streamReady(m_streamUrl, codec, quality);
        setLoading(false);
        setStatusMessage("Playing (" + quality + ")");

        // Actually play the audio
        playUrl(m_streamUrl);
    }

    // ── get_album ──
    else if (cmd == "get_album") {
        QJsonObject data = response["data"].toObject();
        QVariantMap albumInfo = data["album"].toObject().toVariantMap();
        QVariantList tracks;
        for (const QJsonValue &val : data["tracks"].toArray()) {
            tracks.append(val.toObject().toVariantMap());
        }
        emit albumReceived(albumInfo, tracks);
        setLoading(false);
    }

    // ── get_artist ──
    else if (cmd == "get_artist") {
        QJsonObject data = response["data"].toObject();
        QVariantMap artistInfo = data["artist"].toObject().toVariantMap();
        QVariantList topTracks;
        for (const QJsonValue &val : data["top_tracks"].toArray()) {
            topTracks.append(val.toObject().toVariantMap());
        }
        QVariantList albums;
        for (const QJsonValue &val : data["albums"].toArray()) {
            albums.append(val.toObject().toVariantMap());
        }
        emit artistReceived(artistInfo, topTracks, albums);
        setLoading(false);
    }

    // ── get_playlist ──
    else if (cmd == "get_playlist") {
        QJsonObject data = response["data"].toObject();
        QVariantMap playlistInfo = data["playlist"].toObject().toVariantMap();
        QVariantList tracks;
        for (const QJsonValue &val : data["tracks"].toArray()) {
            tracks.append(val.toObject().toVariantMap());
        }
        emit playlistReceived(playlistInfo, tracks);
        setLoading(false);
    }

    // ── favorites ──
    else if (cmd == "favorites") {
        QVariantList tracks;
        for (const QJsonValue &val : response["data"].toArray()) {
            tracks.append(val.toObject().toVariantMap());
        }
        emit favoritesReceived(tracks);
        setLoading(false);
        setStatusMessage(QString::number(tracks.size()) + " favorites");
    }

    // ── home ──
    else if (cmd == "home") {
        QVariantList mixes;
        for (const QJsonValue &val : response["data"].toArray()) {
            mixes.append(val.toObject().toVariantMap());
        }
        emit homeReceived(mixes);
        setLoading(false);
    }

    // ── add_favorite ──
    else if (cmd == "add_favorite") {
        int trackId = response["track_id"].toInt();
        emit favoriteAdded(trackId);
        qDebug() << "TidalClient: Added track to favorites:" << trackId;
    }

    // ── remove_favorite ──
    else if (cmd == "remove_favorite") {
        int trackId = response["track_id"].toInt();
        emit favoriteRemoved(trackId);
        qDebug() << "TidalClient: Removed track from favorites:" << trackId;
    }

    // ── ping ──
    else if (cmd == "ping") {
        // Connection alive
    }
}

// ========================================================================
// HELPERS
// ========================================================================

void TidalClient::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void TidalClient::setLoading(bool loading)
{
    if (m_isLoading != loading) {
        m_isLoading = loading;
        emit loadingChanged();
    }
}
