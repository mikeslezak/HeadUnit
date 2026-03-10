#include "SpotifyClient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QDateTime>
#include <QRandomGenerator>

SpotifyClient::SpotifyClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_serviceProcess(nullptr)
    , m_reconnectTimer(new QTimer(this))
    , m_reconnectAttempts(0)
    , m_authCheckTimer(new QTimer(this))
    , m_positionTimer(new QTimer(this))
    , m_lastPollPosition(0)
    , m_lastPollTime(0)
    , m_playbackPollTimer(new QTimer(this))
    , m_isConnected(false)
    , m_isLoggedIn(false)
    , m_isLoading(false)
    , m_isPlaying(false)
    , m_trackDuration(0)
    , m_position(0)
    , m_duration(0)
    , m_isExplicit(false)
    , m_queuePosition(-1)
    , m_shuffleEnabled(false)
    , m_repeatMode(0)
{
    qDebug() << "SpotifyClient: Initializing...";

    // Socket signals
    connect(m_socket, &QLocalSocket::connected,
            this, &SpotifyClient::onSocketConnected);
    connect(m_socket, &QLocalSocket::disconnected,
            this, &SpotifyClient::onSocketDisconnected);
    connect(m_socket, &QLocalSocket::errorOccurred,
            this, &SpotifyClient::onSocketError);
    connect(m_socket, &QLocalSocket::readyRead,
            this, &SpotifyClient::onSocketReadyRead);

    // Reconnect timer
    m_reconnectTimer->setInterval(RECONNECT_INTERVAL_MS);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &SpotifyClient::onReconnectTimer);

    // Auth check polling timer
    m_authCheckTimer->setInterval(AUTH_CHECK_INTERVAL_MS);
    connect(m_authCheckTimer, &QTimer::timeout, this, [this]() {
        checkLogin();
    });

    // Position interpolation timer (runs locally between API polls)
    m_positionTimer->setInterval(POSITION_UPDATE_MS);
    connect(m_positionTimer, &QTimer::timeout,
            this, &SpotifyClient::onPositionTimer);

    // Playback state polling (polls the Spotify API via service)
    m_playbackPollTimer->setInterval(PLAYBACK_POLL_MS);
    connect(m_playbackPollTimer, &QTimer::timeout,
            this, &SpotifyClient::onPlaybackPollTimer);

    setStatusMessage("Initializing...");
    qDebug() << "SpotifyClient: Initialization complete";
}

SpotifyClient::~SpotifyClient()
{
    m_authCheckTimer->stop();
    m_reconnectTimer->stop();
    m_positionTimer->stop();
    m_playbackPollTimer->stop();

    if (m_socket->state() == QLocalSocket::ConnectedState) {
        m_socket->disconnectFromServer();
    }

    if (m_serviceProcess) {
        m_serviceProcess->terminate();
        m_serviceProcess->waitForFinished(3000);
    }
}

// ========================================================================
// POSITION INTERPOLATION
// ========================================================================

void SpotifyClient::onPositionTimer()
{
    if (!m_isPlaying) return;

    // Interpolate position between API polls
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = now - m_lastPollTime;
    qint64 interpolatedPos = m_lastPollPosition + elapsed;

    if (interpolatedPos != m_position) {
        m_position = qMin(interpolatedPos, m_duration);
        emit positionChanged();
    }
}

void SpotifyClient::onPlaybackPollTimer()
{
    // Poll current playback state from Spotify API
    QJsonObject cmd;
    cmd["cmd"] = "now_playing";
    sendCommand(cmd);
}

// ========================================================================
// SERVICE MANAGEMENT
// ========================================================================

void SpotifyClient::startService()
{
    if (m_serviceProcess) {
        if (m_serviceProcess->state() == QProcess::Running) {
            qDebug() << "SpotifyClient: Service already running";
            return;
        }
        m_serviceProcess->deleteLater();
    }

    m_serviceProcess = new QProcess(this);
    connect(m_serviceProcess, &QProcess::started,
            this, &SpotifyClient::onServiceStarted);
    connect(m_serviceProcess, &QProcess::errorOccurred,
            this, &SpotifyClient::onServiceError);

    connect(m_serviceProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray out = m_serviceProcess->readAllStandardOutput();
        qDebug() << "SpotifyService:" << out.trimmed();
    });
    connect(m_serviceProcess, &QProcess::readyReadStandardError, this, [this]() {
        QByteArray err = m_serviceProcess->readAllStandardError();
        qDebug() << "SpotifyService(err):" << err.trimmed();
    });

    QString projectDir = QCoreApplication::applicationDirPath() + "/..";
    QString scriptPath = QDir(projectDir).absoluteFilePath("services/spotify_service.py");

    if (!QFile::exists(scriptPath)) {
        qWarning() << "SpotifyClient: Service script not found:" << scriptPath;
        setStatusMessage("Service script not found");
        emit error("Spotify service script not found: " + scriptPath);
        return;
    }

    qDebug() << "SpotifyClient: Starting service:" << scriptPath;
    setStatusMessage("Starting Spotify service...");
    m_serviceProcess->start("python3", {scriptPath});
}

void SpotifyClient::connectToService()
{
    if (m_socket->state() == QLocalSocket::ConnectedState) {
        return;
    }

    qDebug() << "SpotifyClient: Connecting to" << SOCKET_PATH;
    setStatusMessage("Connecting...");
    m_socket->connectToServer(SOCKET_PATH);
}

void SpotifyClient::disconnectFromService()
{
    m_reconnectTimer->stop();
    m_authCheckTimer->stop();
    m_playbackPollTimer->stop();
    m_positionTimer->stop();
    m_reconnectAttempts = 0;

    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        m_socket->disconnectFromServer();
    }
}

// ========================================================================
// AUTHENTICATION
// ========================================================================

void SpotifyClient::checkAuthStatus()
{
    QJsonObject cmd;
    cmd["cmd"] = "auth_status";
    sendCommand(cmd);
}

void SpotifyClient::startLogin()
{
    setLoading(true);
    setStatusMessage("Starting login flow...");

    QJsonObject cmd;
    cmd["cmd"] = "auth_login";
    sendCommand(cmd);
}

void SpotifyClient::checkLogin()
{
    QJsonObject cmd;
    cmd["cmd"] = "auth_check";
    sendCommand(cmd);
}

// ========================================================================
// SEARCH
// ========================================================================

void SpotifyClient::search(const QString &query, const QString &type, int limit)
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
// PLAYBACK (via Spotify Connect API)
// ========================================================================

void SpotifyClient::playTrack(const QString &trackId)
{
    setLoading(true);

    QJsonObject cmd;
    cmd["cmd"] = "play_track";
    cmd["track_id"] = trackId;
    sendCommand(cmd);
}

void SpotifyClient::playTrackUri(const QString &uri)
{
    setLoading(true);

    QJsonObject cmd;
    cmd["cmd"] = "play_track";
    cmd["uri"] = uri;
    sendCommand(cmd);
}

void SpotifyClient::playTrackInContext(const QString &trackId, const QVariantList &trackList, int index)
{
    // Set the queue to the provided track list
    m_queue = trackList;
    m_queuePosition = index;

    emit queueChanged();
    emit queuePositionChanged();

    // Set metadata from queue item
    setTrackFromQueue(index);

    // Build list of URIs and play them
    QJsonArray uris;
    for (int i = 0; i < trackList.size(); i++) {
        QVariantMap track = trackList[i].toMap();
        QString uri = track.value("uri").toString();
        if (uri.isEmpty()) {
            uri = "spotify:track:" + track.value("id").toString();
        }
        uris.append(uri);
    }

    QJsonObject cmd;
    cmd["cmd"] = "play_tracks";
    cmd["uris"] = uris;
    sendCommand(cmd);

    // If not starting from first track, seek to the right position
    if (index > 0) {
        QTimer::singleShot(500, this, [this, index]() {
            for (int i = 0; i < index; i++) {
                QJsonObject nextCmd;
                nextCmd["cmd"] = "next";
                sendCommand(nextCmd);
            }
        });
    }
}

void SpotifyClient::playContext(const QString &contextUri, int offset)
{
    setLoading(true);

    QJsonObject cmd;
    cmd["cmd"] = "play_context";
    cmd["context_uri"] = contextUri;
    cmd["offset"] = offset;
    sendCommand(cmd);
}

void SpotifyClient::play()
{
    resume();
}

void SpotifyClient::pause()
{
    QJsonObject cmd;
    cmd["cmd"] = "pause";
    sendCommand(cmd);

    m_isPlaying = false;
    emit playStateChanged();
    m_positionTimer->stop();
}

void SpotifyClient::resume()
{
    QJsonObject cmd;
    cmd["cmd"] = "resume";
    sendCommand(cmd);

    m_isPlaying = true;
    m_lastPollTime = QDateTime::currentMSecsSinceEpoch();
    m_lastPollPosition = m_position;
    emit playStateChanged();
    m_positionTimer->start();
}

void SpotifyClient::stop()
{
    pause();
    m_position = 0;
    emit positionChanged();
}

void SpotifyClient::next()
{
    QJsonObject cmd;
    cmd["cmd"] = "next";
    sendCommand(cmd);

    // Queue position advances
    if (!m_queue.isEmpty()) {
        m_queuePosition++;
        if (m_queuePosition >= m_queue.size()) {
            m_queuePosition = 0;
        }
        emit queuePositionChanged();
        setTrackFromQueue(m_queuePosition);
    }

    m_position = 0;
    emit positionChanged();
}

void SpotifyClient::previous()
{
    // If more than 3 seconds in, restart current track
    if (m_position > 3000) {
        seekTo(0);
        return;
    }

    QJsonObject cmd;
    cmd["cmd"] = "previous";
    sendCommand(cmd);

    if (!m_queue.isEmpty()) {
        m_queuePosition--;
        if (m_queuePosition < 0) {
            m_queuePosition = m_queue.size() - 1;
        }
        emit queuePositionChanged();
        setTrackFromQueue(m_queuePosition);
    }

    m_position = 0;
    emit positionChanged();
}

void SpotifyClient::seekTo(qint64 positionMs)
{
    QJsonObject cmd;
    cmd["cmd"] = "seek";
    cmd["position_ms"] = positionMs;
    sendCommand(cmd);

    // Optimistic update
    m_position = positionMs;
    m_lastPollPosition = positionMs;
    m_lastPollTime = QDateTime::currentMSecsSinceEpoch();
    emit positionChanged();
}

void SpotifyClient::toggleShuffle()
{
    m_shuffleEnabled = !m_shuffleEnabled;

    QJsonObject cmd;
    cmd["cmd"] = "shuffle";
    cmd["enabled"] = m_shuffleEnabled;
    sendCommand(cmd);

    emit shuffleChanged();
}

void SpotifyClient::cycleRepeatMode()
{
    m_repeatMode = (m_repeatMode + 1) % 3;

    QJsonObject cmd;
    cmd["cmd"] = "repeat";
    // Spotify repeat modes: 'off', 'context', 'track'
    QString mode = "off";
    if (m_repeatMode == 1) mode = "context";
    else if (m_repeatMode == 2) mode = "track";
    cmd["mode"] = mode;
    sendCommand(cmd);

    emit repeatModeChanged();
}

// ========================================================================
// QUEUE
// ========================================================================

void SpotifyClient::addToQueue(const QVariantList &tracks)
{
    m_queue.append(tracks);
    emit queueChanged();
}

void SpotifyClient::clearQueue()
{
    m_queue.clear();
    m_queuePosition = -1;
    emit queueChanged();
    emit queuePositionChanged();
}

void SpotifyClient::removeFromQueue(int index)
{
    if (index < 0 || index >= m_queue.size()) return;

    m_queue.removeAt(index);

    if (index < m_queuePosition) {
        m_queuePosition--;
        emit queuePositionChanged();
    } else if (index == m_queuePosition) {
        stop();
    }

    emit queueChanged();
}

// ========================================================================
// QUEUE HELPERS
// ========================================================================

void SpotifyClient::setTrackFromQueue(int index)
{
    if (index < 0 || index >= m_queue.size()) return;

    QVariantMap track = m_queue[index].toMap();
    m_currentTrackUri = track.value("uri").toString();
    m_trackTitle = track.value("title").toString();
    m_artist = track.value("artist").toString();
    m_album = track.value("album").toString();
    m_albumArtUrl = track.value("image_url").toString();
    m_trackDuration = track.value("duration").toInt();
    m_isExplicit = track.value("explicit").toBool();
    m_position = 0;
    m_duration = track.value("duration_ms").toLongLong();
    if (m_duration == 0) {
        m_duration = m_trackDuration * 1000; // seconds to ms
    }

    emit trackChanged();
    emit positionChanged();
    emit durationChanged();
}

void SpotifyClient::playQueueItem(int index)
{
    if (index < 0 || index >= m_queue.size()) return;

    QVariantMap track = m_queue[index].toMap();
    QString uri = track.value("uri").toString();
    if (uri.isEmpty()) {
        uri = "spotify:track:" + track.value("id").toString();
    }
    playTrackUri(uri);
}

// ========================================================================
// BROWSING
// ========================================================================

void SpotifyClient::getAlbum(const QString &albumId)
{
    setLoading(true);
    QJsonObject cmd;
    cmd["cmd"] = "get_album";
    cmd["album_id"] = albumId;
    sendCommand(cmd);
}

void SpotifyClient::getArtist(const QString &artistId)
{
    setLoading(true);
    QJsonObject cmd;
    cmd["cmd"] = "get_artist";
    cmd["artist_id"] = artistId;
    sendCommand(cmd);
}

void SpotifyClient::getPlaylist(const QString &playlistId)
{
    setLoading(true);
    QJsonObject cmd;
    cmd["cmd"] = "get_playlist";
    cmd["playlist_id"] = playlistId;
    sendCommand(cmd);
}

void SpotifyClient::getFavorites()
{
    setLoading(true);
    setStatusMessage("Loading liked songs...");
    QJsonObject cmd;
    cmd["cmd"] = "favorites";
    sendCommand(cmd);
}

void SpotifyClient::getPlaylists()
{
    setLoading(true);
    setStatusMessage("Loading playlists...");
    QJsonObject cmd;
    cmd["cmd"] = "playlists";
    sendCommand(cmd);
}

// ========================================================================
// FAVORITES
// ========================================================================

void SpotifyClient::addFavorite(const QString &trackId)
{
    QJsonObject cmd;
    cmd["cmd"] = "add_favorite";
    cmd["track_id"] = trackId;
    sendCommand(cmd);
}

void SpotifyClient::removeFavorite(const QString &trackId)
{
    QJsonObject cmd;
    cmd["cmd"] = "remove_favorite";
    cmd["track_id"] = trackId;
    sendCommand(cmd);
}

// ========================================================================
// DEVICE
// ========================================================================

void SpotifyClient::findDevice()
{
    QJsonObject cmd;
    cmd["cmd"] = "find_device";
    sendCommand(cmd);
}

// ========================================================================
// SOCKET EVENTS
// ========================================================================

void SpotifyClient::onSocketConnected()
{
    qDebug() << "SpotifyClient: Connected to service";
    m_isConnected = true;
    m_reconnectAttempts = 0;
    m_reconnectTimer->stop();
    emit connectionChanged();
    setStatusMessage("Connected");
    checkAuthStatus();
}

void SpotifyClient::onSocketDisconnected()
{
    qDebug() << "SpotifyClient: Disconnected from service";
    m_isConnected = false;
    m_isLoggedIn = false;
    m_playbackPollTimer->stop();
    m_positionTimer->stop();
    emit connectionChanged();
    emit authStatusChanged();
    setStatusMessage("Disconnected");

    if (m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        m_reconnectTimer->start();
    }
}

void SpotifyClient::onSocketError(QLocalSocket::LocalSocketError socketError)
{
    Q_UNUSED(socketError)

    if (m_isConnected) {
        qWarning() << "SpotifyClient: Socket error:" << m_socket->errorString();
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

void SpotifyClient::onSocketReadyRead()
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
            qWarning() << "SpotifyClient: JSON parse error:" << parseError.errorString();
            continue;
        }

        handleResponse(doc.object());
    }
}

void SpotifyClient::onReconnectTimer()
{
    m_reconnectAttempts++;

    if (m_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        m_reconnectTimer->stop();
        setStatusMessage("Service unavailable - check spotify_service.py");
        return;
    }

    connectToService();
}

void SpotifyClient::onServiceStarted()
{
    qDebug() << "SpotifyClient: Service process started";
    setStatusMessage("Service started, connecting...");
    QTimer::singleShot(1000, this, &SpotifyClient::connectToService);
}

void SpotifyClient::onServiceError(QProcess::ProcessError error)
{
    qWarning() << "SpotifyClient: Service process error:" << error;
    setStatusMessage("Failed to start Spotify service");
    emit this->error("Failed to start Spotify service process");
}

// ========================================================================
// PROTOCOL
// ========================================================================

void SpotifyClient::sendCommand(const QJsonObject &cmd)
{
    if (m_socket->state() != QLocalSocket::ConnectedState) {
        qWarning() << "SpotifyClient: Not connected, can't send:" << cmd["cmd"].toString();
        emit error("Not connected to Spotify service");
        return;
    }

    QJsonDocument doc(cmd);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
    m_socket->flush();
}

void SpotifyClient::handleResponse(const QJsonObject &response)
{
    QString cmd = response["cmd"].toString();
    bool ok = response["ok"].toBool();

    if (!ok) {
        QString errorMsg = response["error"].toString();
        qWarning() << "SpotifyClient: Command failed:" << cmd << errorMsg;
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
            // Start playback state polling
            if (!m_playbackPollTimer->isActive()) {
                m_playbackPollTimer->start();
            }
            // Try to find librespot device
            findDevice();
        } else {
            setStatusMessage("Not logged in");
        }
    }

    // ── auth_login ──
    else if (cmd == "auth_login") {
        QJsonObject data = response["data"].toObject();
        m_loginUrl = data["auth_url"].toString();
        emit loginUrlChanged();
        setLoading(false);
        setStatusMessage("Visit the URL to log in");
        m_authCheckTimer->start();
    }

    // ── auth_check ──
    else if (cmd == "auth_check") {
        bool loggedIn = response["logged_in"].toBool();
        if (loggedIn) {
            m_isLoggedIn = true;
            m_authCheckTimer->stop();
            m_loginUrl.clear();
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

    // ── play_track / play_tracks / play_context ──
    else if (cmd == "play_track" || cmd == "play_tracks" || cmd == "play_context") {
        setLoading(false);
        m_isPlaying = true;
        m_position = 0;
        m_lastPollPosition = 0;
        m_lastPollTime = QDateTime::currentMSecsSinceEpoch();
        emit playStateChanged();
        emit positionChanged();
        m_positionTimer->start();
        setStatusMessage("Playing");
    }

    // ── pause / resume ──
    else if (cmd == "pause") {
        m_isPlaying = false;
        emit playStateChanged();
        m_positionTimer->stop();
    }
    else if (cmd == "resume") {
        m_isPlaying = true;
        m_lastPollTime = QDateTime::currentMSecsSinceEpoch();
        m_lastPollPosition = m_position;
        emit playStateChanged();
        m_positionTimer->start();
    }

    // ── next / previous ──
    else if (cmd == "next" || cmd == "previous") {
        m_position = 0;
        m_lastPollPosition = 0;
        m_lastPollTime = QDateTime::currentMSecsSinceEpoch();
        emit positionChanged();
        // Track info will update on next playback poll
    }

    // ── seek ──
    else if (cmd == "seek") {
        // Position already optimistically updated
    }

    // ── now_playing (playback state poll) ──
    else if (cmd == "now_playing") {
        QJsonObject data = response["data"].toObject();
        bool wasPlaying = m_isPlaying;
        m_isPlaying = data["is_playing"].toBool();

        if (wasPlaying != m_isPlaying) {
            emit playStateChanged();
            if (m_isPlaying) {
                m_positionTimer->start();
            } else {
                m_positionTimer->stop();
            }
        }

        // Update position from poll
        qint64 pollPos = data["progress_ms"].toVariant().toLongLong();
        m_lastPollPosition = pollPos;
        m_lastPollTime = QDateTime::currentMSecsSinceEpoch();
        m_position = pollPos;
        emit positionChanged();

        // Update shuffle/repeat state from server
        bool serverShuffle = data["shuffle"].toBool();
        if (serverShuffle != m_shuffleEnabled) {
            m_shuffleEnabled = serverShuffle;
            emit shuffleChanged();
        }

        QString repeatState = data["repeat"].toString();
        int serverRepeat = 0;
        if (repeatState == "context") serverRepeat = 1;
        else if (repeatState == "track") serverRepeat = 2;
        if (serverRepeat != m_repeatMode) {
            m_repeatMode = serverRepeat;
            emit repeatModeChanged();
        }

        // Update track info if changed
        QString trackUri = data["track_uri"].toString();
        if (!trackUri.isEmpty() && trackUri != m_currentTrackUri) {
            m_currentTrackUri = trackUri;
            m_trackTitle = data["title"].toString();
            m_artist = data["artist"].toString();
            m_album = data["album"].toString();
            m_albumArtUrl = data["image_url"].toString();
            m_isExplicit = data["explicit"].toBool();
            m_duration = data["duration_ms"].toVariant().toLongLong();
            m_trackDuration = m_duration / 1000;
            emit trackChanged();
            emit durationChanged();
        }
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
        setStatusMessage(QString::number(tracks.size()) + " liked songs");
    }

    // ── playlists ──
    else if (cmd == "playlists") {
        QVariantList playlists;
        for (const QJsonValue &val : response["data"].toArray()) {
            playlists.append(val.toObject().toVariantMap());
        }
        emit playlistsReceived(playlists);
        setLoading(false);
    }

    // ── add_favorite ──
    else if (cmd == "add_favorite") {
        QString trackId = response["track_id"].toString();
        emit favoriteAdded(trackId);
    }

    // ── remove_favorite ──
    else if (cmd == "remove_favorite") {
        QString trackId = response["track_id"].toString();
        emit favoriteRemoved(trackId);
    }

    // ── find_device ──
    else if (cmd == "find_device") {
        QString deviceId = response["device_id"].toString();
        if (!deviceId.isEmpty()) {
            qDebug() << "SpotifyClient: Found HeadUnit device:" << deviceId;
        }
    }

    // ── ping ──
    else if (cmd == "ping") {
        // Connection alive
    }
}

// ========================================================================
// HELPERS
// ========================================================================

void SpotifyClient::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

void SpotifyClient::setLoading(bool loading)
{
    if (m_isLoading != loading) {
        m_isLoading = loading;
        emit loadingChanged();
    }
}
