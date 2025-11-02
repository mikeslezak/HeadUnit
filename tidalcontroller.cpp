#include "tidalcontroller.h"
#include <QStandardPaths>
#include <QRegularExpression>
#include <QRandomGenerator>

const QString TidalController::TIDAL_PACKAGE = "com.aspiro.tidal";
const QString TidalController::TIDAL_ACTIVITY = "com.aspiro.tidal/com.aspiro.tidal.ui.TidalActivity";

TidalController::TidalController(QObject *parent)
    : QObject(parent)
    , m_adbProcess(new QProcess(this))
    , m_pollTimer(new QTimer(this))
    , m_progressTimer(new QTimer(this))
    , m_isPlaying(false)
    , m_isConnected(false)
    , m_currentTrack("No track loaded")
    , m_currentArtist("")
    , m_currentAlbum("")
    , m_albumArtUrl("")
    , m_statusMessage("Initializing...")
    , m_trackPosition(0)
    , m_trackDuration(0)
    , m_isSearching(false)
{
#ifdef Q_OS_WIN
    m_mockMode = true;
    qDebug() << "TidalController: Running in MOCK mode (Windows)";
    setStatusMessage("Mock Mode - Ready for Testing");

    QTimer::singleShot(2000, this, [this]() {
        m_isConnected = true;
        emit connectionChanged();
        setStatusMessage("Mock: Ready");

        simulateTrackChange("Blinding Lights", "The Weeknd", "After Hours");
        m_trackDuration = 200000;
        emit trackDurationChanged();

        generateMockPlaylists();
        generateMockRecentlyPlayed();
        generateMockDownloads();
        generateMockFavorites();
    });

    connect(m_progressTimer, &QTimer::timeout, this, &TidalController::updateTrackPosition);
    m_progressTimer->setInterval(1000);
#else
    m_mockMode = false;
    qDebug() << "TidalController: Headless Android Integration Mode";

    connect(m_adbProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TidalController::onAdbCommandFinished);
    connect(m_adbProcess, &QProcess::errorOccurred,
            this, &TidalController::onAdbError);

    // Poll MediaSession every 2 seconds for state
    // connect(m_pollTimer, &QTimer::timeout, this, &TidalController::pollMediaSession);
    // m_pollTimer->start(2000);

    connect(m_progressTimer, &QTimer::timeout, this, &TidalController::updateTrackPosition);
    m_progressTimer->setInterval(1000);

    // QTimer::singleShot(500, this, &TidalController::checkConnection);
    // QTimer::singleShot(3000, this, &TidalController::syncWithAndroidApp);
#endif
}

TidalController::~TidalController()
{
    if (m_adbProcess && m_adbProcess->state() == QProcess::Running) {
        m_adbProcess->kill();
        m_adbProcess->waitForFinished(1000);
    }
}

void TidalController::setStatusMessage(const QString &msg)
{
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
        qDebug() << "TidalController:" << msg;
    }
}

void TidalController::updateTrackPosition()
{
    if (m_isPlaying && m_trackPosition < m_trackDuration) {
        m_trackPosition += 1000;
        emit trackPositionChanged();

        if (m_trackPosition >= m_trackDuration) {
            advanceQueue();
        }
    }
}

void TidalController::advanceQueue()
{
    if (!m_queue.isEmpty()) {
        QVariantMap nextTrack = m_queue.first().toMap();
        m_queue.removeFirst();
        emit queueChanged();

        loadMockTrack(nextTrack);
        simulatePlay();
    } else {
        next();
    }
}

void TidalController::sendAdbCommand(const QString &command, const QStringList &args)
{
#ifdef Q_OS_WIN
    qDebug() << "MOCK ADB:" << command << args;
    return;
#else
    if (m_adbProcess->state() == QProcess::Running) {
        qWarning() << "ADB process already running, queuing command";
        return;
    }

    QStringList fullArgs;
    fullArgs << "-s" << "waydroid" << command;
    fullArgs.append(args);

    qDebug() << "ADB command:" << fullArgs.join(" ");

    m_adbProcess->start("adb", fullArgs);

    if (!m_adbProcess->waitForStarted(3000)) {
        emit error("Failed to start ADB command");
        setStatusMessage("Error: ADB not responding");
    }
#endif
}

// Playback Controls
void TidalController::play()
{
#ifdef Q_OS_WIN
    simulatePlay();
#else
    sendAdbCommand("shell", {"input", "keyevent", "126"}); // KEYCODE_MEDIA_PLAY
#endif
}

void TidalController::pause()
{
#ifdef Q_OS_WIN
    simulatePause();
#else
    sendAdbCommand("shell", {"input", "keyevent", "127"}); // KEYCODE_MEDIA_PAUSE
#endif
}

void TidalController::togglePlayPause()
{
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

void TidalController::next()
{
#ifdef Q_OS_WIN
    if (!m_queue.isEmpty()) {
        advanceQueue();
        return;
    }

    QStringList tracks = {
        "Starboy|The Weeknd|Starboy",
        "One Dance|Drake|Views",
        "Shape of You|Ed Sheeran|÷",
        "Levitating|Dua Lipa|Future Nostalgia"
    };

    QString track = tracks.at(QRandomGenerator::global()->bounded(tracks.size()));
    QStringList parts = track.split("|");
    simulateTrackChange(parts[0], parts[1], parts[2]);
#else
    sendAdbCommand("shell", {"input", "keyevent", "87"}); // KEYCODE_MEDIA_NEXT
#endif
}

void TidalController::previous()
{
#ifdef Q_OS_WIN
    if (m_trackPosition > 5000) {
        m_trackPosition = 0;
        emit trackPositionChanged();
    } else {
        simulateTrackChange("Blinding Lights", "The Weeknd", "After Hours");
    }
#else
    sendAdbCommand("shell", {"input", "keyevent", "88"}); // KEYCODE_MEDIA_PREVIOUS
#endif
}

void TidalController::seekTo(int positionMs)
{
#ifdef Q_OS_WIN
    if (positionMs >= 0 && positionMs <= m_trackDuration) {
        m_trackPosition = positionMs;
        emit trackPositionChanged();
    }
#else
    // MediaSession seeking - more complex, requires broadcast intent
    qDebug() << "Seeking to" << positionMs;
#endif
}

// Search
void TidalController::search(const QString &query)
{
    if (query.trimmed().isEmpty()) {
        m_searchResults.clear();
        emit searchResultsChanged();
        return;
    }

#ifdef Q_OS_WIN
    m_isSearching = true;
    emit isSearchingChanged();
    setStatusMessage("Searching for: " + query);

    QTimer::singleShot(800, this, [this, query]() {
        generateMockSearchResults(query);
        m_isSearching = false;
        emit isSearchingChanged();
        setStatusMessage("Found " + QString::number(m_searchResults.size()) + " results");
    });
#else
    // Launch TIDAL search via intent
    m_isSearching = true;
    emit isSearchingChanged();
    setStatusMessage("Searching via Android app: " + query);

    sendAdbCommand("shell", {"am", "start", "-a", "android.intent.action.SEARCH",
                             "-n", TIDAL_ACTIVITY,
                             "--es", "query", query});

    // Mock results for now - real implementation would need content provider query
    QTimer::singleShot(1000, this, [this, query]() {
        generateMockSearchResults(query);
        m_isSearching = false;
        emit isSearchingChanged();
    });
#endif
}

void TidalController::playTrack(const QString &trackId)
{
#ifdef Q_OS_WIN
    for (const QVariant &result : m_searchResults) {
        QVariantMap track = result.toMap();
        if (track["id"].toString() == trackId) {
            loadMockTrack(track);
            addToRecentlyPlayed(track);
            simulatePlay();
            return;
        }
    }
#else
    // Launch track via deep link
    QString tidalUrl = QString("tidal://track/%1").arg(trackId);
    sendAdbCommand("shell", {"am", "start", "-a", "android.intent.action.VIEW",
                             "-d", tidalUrl});
    setStatusMessage("Playing track: " + trackId);
#endif
}

// Library Management
void TidalController::loadPlaylists()
{
#ifdef Q_OS_WIN
    setStatusMessage("Loading playlists...");
    QTimer::singleShot(500, this, [this]() {
        generateMockPlaylists();
        setStatusMessage("Playlists loaded");
    });
#else
    setStatusMessage("Syncing playlists from Android app...");
    m_pendingQueryPurpose = "playlists";

    // Query TIDAL app's database for playlists
    queryAndroidAppDatabase("SELECT * FROM playlists LIMIT 50", "playlists");
#endif
}

void TidalController::loadFavorites()
{
#ifdef Q_OS_WIN
    setStatusMessage("Loading favorites...");
    QTimer::singleShot(500, this, [this]() {
        generateMockFavorites();
        setStatusMessage("Favorites loaded");
    });
#else
    setStatusMessage("Syncing favorites from Android app...");
    m_pendingQueryPurpose = "favorites";

    queryAndroidAppDatabase("SELECT * FROM favorites WHERE type='TRACK'", "favorites");
#endif
}

void TidalController::loadDownloads()
{
#ifdef Q_OS_WIN
    setStatusMessage("Loading downloads...");
    QTimer::singleShot(500, this, [this]() {
        generateMockDownloads();
        setStatusMessage("Downloads loaded");
    });
#else
    setStatusMessage("Syncing downloads from Android app...");
    m_pendingQueryPurpose = "downloads";

    queryAndroidAppDatabase("SELECT * FROM offline_tracks WHERE status='COMPLETE'", "downloads");
#endif
}

void TidalController::loadRecentlyPlayed()
{
#ifdef Q_OS_WIN
    setStatusMessage("Loading recently played...");
    QTimer::singleShot(500, this, [this]() {
        generateMockRecentlyPlayed();
        setStatusMessage("Recently played loaded");
    });
#else
    setStatusMessage("Syncing recent from Android app...");
    m_pendingQueryPurpose = "recent";

    queryAndroidAppDatabase("SELECT * FROM recent_tracks ORDER BY timestamp DESC LIMIT 50", "recent");
#endif
}

void TidalController::queryAndroidAppDatabase(const QString &query, const QString &purpose)
{
#ifndef Q_OS_WIN
    // Try to query TIDAL's internal database
    // Note: This may require root or TIDAL to expose a content provider
    QStringList args;
    args << "shell" << "run-as" << TIDAL_PACKAGE
         << "sqlite3" << "databases/tidal.db" << query;

    m_pendingQueryPurpose = purpose;
    sendAdbCommand("shell", args);

    // Fallback to mock if query fails
    QTimer::singleShot(3000, this, [this, purpose]() {
        if (purpose == "downloads" && m_downloads.isEmpty()) {
            generateMockDownloads();
        } else if (purpose == "favorites" && m_favorites.isEmpty()) {
            generateMockFavorites();
        } else if (purpose == "playlists" && m_playlists.isEmpty()) {
            generateMockPlaylists();
        }
    });
#endif
}

// Downloads Management
void TidalController::downloadTrack(const QString &trackId)
{
#ifdef Q_OS_WIN
    setStatusMessage("Mock: Downloading track " + trackId);

    // Find track in search results
    for (const QVariant &result : m_searchResults) {
        QVariantMap track = result.toMap();
        if (track["id"].toString() == trackId) {
            m_downloads.prepend(track);
            m_downloadedIds.insert(trackId);
            emit downloadsChanged();
            setStatusMessage("Downloaded: " + track["title"].toString());
            return;
        }
    }
#else
    // Trigger download via broadcast intent to TIDAL app
    sendAdbCommand("shell", {"am", "broadcast",
                             "-a", "com.aspiro.tidal.action.DOWNLOAD_TRACK",
                             "--es", "trackId", trackId});
    setStatusMessage("Downloading track via Android app: " + trackId);
#endif
}

void TidalController::removeDownload(const QString &trackId)
{
#ifdef Q_OS_WIN
    for (int i = 0; i < m_downloads.size(); ++i) {
        if (m_downloads[i].toMap()["id"].toString() == trackId) {
            m_downloads.removeAt(i);
            m_downloadedIds.remove(trackId);
            emit downloadsChanged();
            setStatusMessage("Download removed");
            return;
        }
    }
#else
    sendAdbCommand("shell", {"am", "broadcast",
                             "-a", "com.aspiro.tidal.action.REMOVE_DOWNLOAD",
                             "--es", "trackId", trackId});
    setStatusMessage("Removing download: " + trackId);
#endif
}

bool TidalController::isDownloaded(const QString &trackId)
{
    return m_downloadedIds.contains(trackId);
}

// Favorites Management
void TidalController::addToFavorites(const QString &trackId)
{
#ifdef Q_OS_WIN
    m_favoriteIds.insert(trackId);

    for (const QVariant &result : m_searchResults) {
        QVariantMap track = result.toMap();
        if (track["id"].toString() == trackId) {
            m_favorites.prepend(track);
            emit favoritesChanged();
            setStatusMessage("Added to favorites");
            return;
        }
    }
#else
    sendAdbCommand("shell", {"am", "broadcast",
                             "-a", "com.aspiro.tidal.action.ADD_FAVORITE",
                             "--es", "trackId", trackId,
                             "--es", "type", "TRACK"});
    m_favoriteIds.insert(trackId);
    setStatusMessage("Adding to favorites: " + trackId);
#endif
}

void TidalController::removeFromFavorites(const QString &trackId)
{
#ifdef Q_OS_WIN
    m_favoriteIds.remove(trackId);

    for (int i = 0; i < m_favorites.size(); ++i) {
        if (m_favorites[i].toMap()["id"].toString() == trackId) {
            m_favorites.removeAt(i);
            emit favoritesChanged();
            setStatusMessage("Removed from favorites");
            return;
        }
    }
#else
    sendAdbCommand("shell", {"am", "broadcast",
                             "-a", "com.aspiro.tidal.action.REMOVE_FAVORITE",
                             "--es", "trackId", trackId});
    m_favoriteIds.remove(trackId);
    setStatusMessage("Removing from favorites: " + trackId);
#endif
}

bool TidalController::isFavorite(const QString &trackId)
{
    return m_favoriteIds.contains(trackId);
}

// Queue Management
void TidalController::addToQueue(const QVariantMap &track)
{
    m_queue.append(track);
    emit queueChanged();
    setStatusMessage("Added to queue: " + track["title"].toString());
}

void TidalController::removeFromQueue(int index)
{
    if (index >= 0 && index < m_queue.size()) {
        m_queue.removeAt(index);
        emit queueChanged();
        setStatusMessage("Removed from queue");
    }
}

void TidalController::clearQueue()
{
    m_queue.clear();
    emit queueChanged();
    setStatusMessage("Queue cleared");
}

void TidalController::playFromQueue(int index)
{
    if (index >= 0 && index < m_queue.size()) {
        QVariantMap track = m_queue.at(index).toMap();
        m_queue.removeAt(index);
        emit queueChanged();

        loadMockTrack(track);
        addToRecentlyPlayed(track);
        simulatePlay();
    }
}

// Connection Management
void TidalController::startTidalApp()
{
#ifdef Q_OS_WIN
    setStatusMessage("Mock: Starting TIDAL app");
    m_isConnected = true;
    emit connectionChanged();
#else
    setStatusMessage("Starting TIDAL Android app...");
    sendAdbCommand("shell", {"am", "start", "-n", TIDAL_ACTIVITY});
    // QTimer::singleShot(3000, this, &TidalController::checkConnection);  // DISABLED
#endif
}

void TidalController::stopTidalApp()
{
#ifdef Q_OS_WIN
    setStatusMessage("Mock: Stopping TIDAL app");
    m_isPlaying = false;
    m_isConnected = false;
    m_progressTimer->stop();
    emit playStateChanged();
    emit connectionChanged();
#else
    sendAdbCommand("shell", {"am", "force-stop", TIDAL_PACKAGE});
    m_isPlaying = false;
    m_isConnected = false;
    m_progressTimer->stop();
    emit playStateChanged();
    emit connectionChanged();
    setStatusMessage("TIDAL app stopped");
#endif
}

void TidalController::checkConnection()
{
#ifdef Q_OS_WIN
    return;
#else
    sendAdbCommand("shell", {"dumpsys", "activity", "activities", "|", "grep", TIDAL_PACKAGE});
#endif
}

void TidalController::syncWithAndroidApp()
{
#ifndef Q_OS_WIN
    qDebug() << "Syncing library from Android TIDAL app...";
    loadPlaylists();
    loadFavorites();
    loadDownloads();
    loadRecentlyPlayed();
#endif
}

void TidalController::pollMediaSession()
{
#ifndef Q_OS_WIN
    sendAdbCommand("shell", {"dumpsys", "media_session"});
#endif
}

// ADB Response Handling
void TidalController::onAdbCommandFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        return;
    }

    QString output = QString::fromUtf8(m_adbProcess->readAllStandardOutput());

    // Determine what kind of response this is
    if (output.contains("media_session") || output.contains("MediaSession")) {
        parseMediaSessionState(output);
    } else if (m_pendingQueryPurpose == "downloads") {
        parseDownloadsFromDb(output);
        m_pendingQueryPurpose.clear();
    } else if (m_pendingQueryPurpose == "favorites") {
        parseFavoritesFromDb(output);
        m_pendingQueryPurpose.clear();
    } else if (m_pendingQueryPurpose == "playlists") {
        parsePlaylistsFromDb(output);
        m_pendingQueryPurpose.clear();
    } else if (output.contains(TIDAL_PACKAGE)) {
        m_isConnected = true;
        emit connectionChanged();
    }
}

void TidalController::parseMediaSessionState(const QString &output)
{
    // Parse MediaSession dump for current state
    bool wasPlaying = m_isPlaying;
    QString oldTrack = m_currentTrack;

    // Look for playback state
    if (output.contains("state=3") || output.contains("STATE_PLAYING")) {
        m_isPlaying = true;
        if (!m_progressTimer->isActive()) {
            m_progressTimer->start();
        }
    } else if (output.contains("state=2") || output.contains("STATE_PAUSED")) {
        m_isPlaying = false;
        m_progressTimer->stop();
    }

    // Extract metadata
    QRegularExpression titleRegex("title=([^,\\n]+)");
    QRegularExpressionMatch titleMatch = titleRegex.match(output);
    if (titleMatch.hasMatch()) {
        QString newTitle = titleMatch.captured(1).trimmed();
        if (!newTitle.isEmpty() && newTitle != m_currentTrack) {
            m_currentTrack = newTitle;
        }
    }

    QRegularExpression artistRegex("artist=([^,\\n]+)");
    QRegularExpressionMatch artistMatch = artistRegex.match(output);
    if (artistMatch.hasMatch()) {
        m_currentArtist = artistMatch.captured(1).trimmed();
    }

    QRegularExpression albumRegex("album=([^,\\n]+)");
    QRegularExpressionMatch albumMatch = albumRegex.match(output);
    if (albumMatch.hasMatch()) {
        m_currentAlbum = albumMatch.captured(1).trimmed();
    }

    // Extract duration and position
    QRegularExpression durationRegex("duration=(\\d+)");
    QRegularExpressionMatch durationMatch = durationRegex.match(output);
    if (durationMatch.hasMatch()) {
        m_trackDuration = durationMatch.captured(1).toInt();
        emit trackDurationChanged();
    }

    QRegularExpression positionRegex("position=(\\d+)");
    QRegularExpressionMatch positionMatch = positionRegex.match(output);
    if (positionMatch.hasMatch()) {
        m_trackPosition = positionMatch.captured(1).toInt();
        emit trackPositionChanged();
    }

    if (wasPlaying != m_isPlaying) {
        emit playStateChanged();
        setStatusMessage(m_isPlaying ? "Playing" : "Paused");
    }

    if (oldTrack != m_currentTrack) {
        emit trackChanged();
    }
}

void TidalController::parseDownloadsFromDb(const QString &output)
{
    // Parse SQLite output for downloaded tracks
    // Format depends on database schema
    qDebug() << "Downloads DB output:" << output;

    // Fallback to mock if parsing fails
    if (output.isEmpty() || output.contains("Error")) {
        generateMockDownloads();
    }
}

void TidalController::parseFavoritesFromDb(const QString &output)
{
    qDebug() << "Favorites DB output:" << output;

    if (output.isEmpty() || output.contains("Error")) {
        generateMockFavorites();
    }
}

void TidalController::parsePlaylistsFromDb(const QString &output)
{
    qDebug() << "Playlists DB output:" << output;

    if (output.isEmpty() || output.contains("Error")) {
        generateMockPlaylists();
    }
}

void TidalController::onAdbError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = "ADB failed to start";
        break;
    case QProcess::Crashed:
        errorMsg = "ADB process crashed";
        break;
    default:
        errorMsg = "ADB error";
        break;
    }

    emit this->error(errorMsg);
    setStatusMessage("Error: " + errorMsg);
}

// Mock Data Generators
void TidalController::generateMockSearchResults(const QString &query)
{
    m_searchResults.clear();

    QList<QVariantMap> mockTracks = {
        {{"id", "251380837"}, {"title", "Blinding Lights"}, {"artist", "The Weeknd"}, {"album", "After Hours"}, {"duration", 200000}, {"albumArt", "https://resources.tidal.com/images/3bd15127/3db5/4e76/9e06/cfdf523bca62/320x320.jpg"}},
        {{"id", "251380838"}, {"title", "Save Your Tears"}, {"artist", "The Weeknd"}, {"album", "After Hours"}, {"duration", 215000}, {"albumArt", "https://resources.tidal.com/images/3bd15127/3db5/4e76/9e06/cfdf523bca62/320x320.jpg"}},
        {{"id", "251380839"}, {"title", "Levitating"}, {"artist", "Dua Lipa"}, {"album", "Future Nostalgia"}, {"duration", 203000}, {"albumArt", "https://resources.tidal.com/images/1f8f5186/c2f3/4cf8/8836/74484b48e00d/320x320.jpg"}},
        {{"id", "251380840"}, {"title", "Starboy"}, {"artist", "The Weeknd"}, {"album", "Starboy"}, {"duration", 230000}, {"albumArt", "https://resources.tidal.com/images/5bf08c63/5c82/46ed/aa9f/e4029b9c79fc/320x320.jpg"}},
        {{"id", "251380841"}, {"title", "One Dance"}, {"artist", "Drake"}, {"album", "Views"}, {"duration", 173000}, {"albumArt", "https://resources.tidal.com/images/1f8f5186/c2f3/4cf8/8836/74484b48e00d/320x320.jpg"}},
        {{"id", "251380842"}, {"title", "Don't Start Now"}, {"artist", "Dua Lipa"}, {"album", "Future Nostalgia"}, {"duration", 183000}, {"albumArt", "https://resources.tidal.com/images/1f8f5186/c2f3/4cf8/8836/74484b48e00d/320x320.jpg"}},
        {{"id", "251380843"}, {"title", "Circles"}, {"artist", "Post Malone"}, {"album", "Hollywood's Bleeding"}, {"duration", 215000}, {"albumArt", "https://resources.tidal.com/images/5bf08c63/5c82/46ed/aa9f/e4029b9c79fc/320x320.jpg"}}
    };

    QString lowerQuery = query.toLower();
    for (const auto &track : mockTracks) {
        QString title = track["title"].toString().toLower();
        QString artist = track["artist"].toString().toLower();

        if (title.contains(lowerQuery) || artist.contains(lowerQuery)) {
            m_searchResults.append(track);
        }
    }

    if (m_searchResults.isEmpty()) {
        for (const auto &track : mockTracks) {
            m_searchResults.append(track);
        }
    }

    emit searchResultsChanged();
}

void TidalController::generateMockPlaylists()
{
    m_playlists.clear();

    m_playlists.append(QVariantMap{
        {"id", "playlist1"},
        {"name", "My Favorites"},
        {"trackCount", 47},
        {"coverArt", "https://resources.tidal.com/images/3bd15127/3db5/4e76/9e06/cfdf523bca62/320x320.jpg"}
    });

    m_playlists.append(QVariantMap{
        {"id", "playlist2"},
        {"name", "Road Trip"},
        {"trackCount", 32},
        {"coverArt", "https://resources.tidal.com/images/1f8f5186/c2f3/4cf8/8836/74484b48e00d/320x320.jpg"}
    });

    m_playlists.append(QVariantMap{
        {"id", "playlist3"},
        {"name", "Workout Mix"},
        {"trackCount", 28},
        {"coverArt", "https://resources.tidal.com/images/5bf08c63/5c82/46ed/aa9f/e4029b9c79fc/320x320.jpg"}
    });

    emit playlistsChanged();
}

void TidalController::generateMockRecentlyPlayed()
{
    m_recentlyPlayed.clear();

    QList<QVariantMap> tracks = {
        {{"id", "r1"}, {"title", "Blinding Lights"}, {"artist", "The Weeknd"}, {"album", "After Hours"}, {"duration", 200000}, {"albumArt", "https://resources.tidal.com/images/3bd15127/3db5/4e76/9e06/cfdf523bca62/320x320.jpg"}},
        {{"id", "r2"}, {"title", "Levitating"}, {"artist", "Dua Lipa"}, {"album", "Future Nostalgia"}, {"duration", 203000}, {"albumArt", "https://resources.tidal.com/images/1f8f5186/c2f3/4cf8/8836/74484b48e00d/320x320.jpg"}},
        {{"id", "r3"}, {"title", "Save Your Tears"}, {"artist", "The Weeknd"}, {"album", "After Hours"}, {"duration", 215000}, {"albumArt", "https://resources.tidal.com/images/3bd15127/3db5/4e76/9e06/cfdf523bca62/320x320.jpg"}}
    };

    for (const auto &track : tracks) {
        m_recentlyPlayed.append(track);
    }

    emit recentlyPlayedChanged();
}

void TidalController::generateMockDownloads()
{
    m_downloads.clear();
    m_downloadedIds.clear();

    QList<QVariantMap> tracks = {
        {{"id", "d1"}, {"title", "Blinding Lights"}, {"artist", "The Weeknd"}, {"album", "After Hours"}, {"duration", 200000}, {"albumArt", "https://resources.tidal.com/images/3bd15127/3db5/4e76/9e06/cfdf523bca62/320x320.jpg"}},
        {{"id", "d2"}, {"title", "Starboy"}, {"artist", "The Weeknd"}, {"album", "Starboy"}, {"duration", 230000}, {"albumArt", "https://resources.tidal.com/images/5bf08c63/5c82/46ed/aa9f/e4029b9c79fc/320x320.jpg"}}
    };

    for (const auto &track : tracks) {
        m_downloads.append(track);
        m_downloadedIds.insert(track["id"].toString());
    }

    emit downloadsChanged();
}

void TidalController::generateMockFavorites()
{
    m_favorites.clear();
    m_favoriteIds.clear();

    QList<QVariantMap> tracks = {
        {{"id", "f1"}, {"title", "Levitating"}, {"artist", "Dua Lipa"}, {"album", "Future Nostalgia"}, {"duration", 203000}, {"albumArt", "https://resources.tidal.com/images/1f8f5186/c2f3/4cf8/8836/74484b48e00d/320x320.jpg"}},
        {{"id", "f2"}, {"title", "Don't Start Now"}, {"artist", "Dua Lipa"}, {"album", "Future Nostalgia"}, {"duration", 183000}, {"albumArt", "https://resources.tidal.com/images/1f8f5186/c2f3/4cf8/8836/74484b48e00d/320x320.jpg"}}
    };

    for (const auto &track : tracks) {
        m_favorites.append(track);
        m_favoriteIds.insert(track["id"].toString());
    }

    emit favoritesChanged();
}

void TidalController::loadMockTrack(const QVariantMap &track)
{
    m_currentTrack = track["title"].toString();
    m_currentArtist = track["artist"].toString();
    m_currentAlbum = track["album"].toString();
    m_albumArtUrl = track["albumArt"].toString();
    m_trackDuration = track["duration"].toInt();
    m_trackPosition = 0;

    emit trackChanged();
    emit trackDurationChanged();
    emit trackPositionChanged();

    setStatusMessage("Loaded: " + m_currentTrack);
}

void TidalController::addToRecentlyPlayed(const QVariantMap &track)
{
    for (int i = 0; i < m_recentlyPlayed.size(); ++i) {
        if (m_recentlyPlayed[i].toMap()["id"] == track["id"]) {
            m_recentlyPlayed.removeAt(i);
            break;
        }
    }

    m_recentlyPlayed.prepend(track);

    while (m_recentlyPlayed.size() > 20) {
        m_recentlyPlayed.removeLast();
    }

    emit recentlyPlayedChanged();
}

void TidalController::loadPlaylist(const QString &playlistId)
{
    qDebug() << "Loading playlist:" << playlistId;
}

void TidalController::simulatePlay()
{
    m_isPlaying = true;
    m_progressTimer->start();
    emit playStateChanged();
    setStatusMessage("Playing: " + m_currentTrack);
}

void TidalController::simulatePause()
{
    m_isPlaying = false;
    m_progressTimer->stop();
    emit playStateChanged();
    setStatusMessage("Paused");
}

void TidalController::simulateTrackChange(const QString &trackName, const QString &artist, const QString &album)
{
    m_currentTrack = trackName;
    m_currentArtist = artist;
    m_currentAlbum = album;

    QStringList artUrls = {
        "https://resources.tidal.com/images/3bd15127/3db5/4e76/9e06/cfdf523bca62/320x320.jpg",
        "https://resources.tidal.com/images/1f8f5186/c2f3/4cf8/8836/74484b48e00d/320x320.jpg",
        "https://resources.tidal.com/images/5bf08c63/5c82/46ed/aa9f/e4029b9c79fc/320x320.jpg"
    };

    m_albumArtUrl = artUrls.at(QRandomGenerator::global()->bounded(artUrls.size()));
    m_trackDuration = 180000 + QRandomGenerator::global()->bounded(120000);
    m_trackPosition = 0;

    emit trackChanged();
    emit trackDurationChanged();
    emit trackPositionChanged();
    setStatusMessage("Loaded: " + trackName);
}
