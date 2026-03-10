#ifndef SPOTIFYCLIENT_H
#define SPOTIFYCLIENT_H

#include <QObject>
#include <QString>
#include <QLocalSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QProcess>

/**
 * SpotifyClient - Bridge between QML and the Python Spotify service
 *
 * Communicates with spotify_service.py over a Unix domain socket
 * using newline-delimited JSON. Playback is handled by librespot
 * (Spotify Connect receiver) — no GStreamer needed.
 *
 * The Python service handles:
 *   - OAuth PKCE authorization flow
 *   - Spotify Web API calls (search, catalog browsing)
 *   - Spotify Connect API calls (playback control)
 *   - librespot process management
 *
 * This C++ class handles:
 *   - Socket connection management
 *   - JSON protocol encoding/decoding
 *   - Position interpolation between API polls
 *   - Queue management
 *   - Exposing everything to QML
 */
class SpotifyClient : public QObject
{
    Q_OBJECT

    // ========== CONNECTION STATE ==========
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY authStatusChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY authStatusChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

    // ========== AUTH FLOW ==========
    Q_PROPERTY(QString loginUrl READ loginUrl NOTIFY loginUrlChanged)

    // ========== NOW PLAYING ==========
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playStateChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString albumArtUrl READ albumArtUrl NOTIFY trackChanged)
    Q_PROPERTY(int trackDuration READ trackDuration NOTIFY trackChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool isExplicit READ isExplicit NOTIFY trackChanged)

    // ========== QUEUE ==========
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int queuePosition READ queuePosition NOTIFY queuePositionChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled NOTIFY shuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode NOTIFY repeatModeChanged)

    // ========== SEARCH ==========
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)

public:
    explicit SpotifyClient(QObject *parent = nullptr);
    ~SpotifyClient();

    // Property getters
    bool isConnected() const { return m_isConnected; }
    bool isLoggedIn() const { return m_isLoggedIn; }
    QString userName() const { return m_userName; }
    bool isLoading() const { return m_isLoading; }
    QString statusMessage() const { return m_statusMessage; }
    QString loginUrl() const { return m_loginUrl; }
    bool isPlaying() const { return m_isPlaying; }
    QString trackTitle() const { return m_trackTitle; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString albumArtUrl() const { return m_albumArtUrl; }
    int trackDuration() const { return m_trackDuration; }
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    bool isExplicit() const { return m_isExplicit; }
    QVariantList queue() const { return m_queue; }
    int queuePosition() const { return m_queuePosition; }
    bool shuffleEnabled() const { return m_shuffleEnabled; }
    int repeatMode() const { return m_repeatMode; }
    QVariantList searchResults() const { return m_searchResults; }

public slots:
    // ========== SERVICE MANAGEMENT ==========
    void connectToService();
    void disconnectFromService();
    void startService();

    // ========== AUTHENTICATION ==========
    void checkAuthStatus();
    void startLogin();
    void checkLogin();

    // ========== SEARCH ==========
    void search(const QString &query, const QString &type = "tracks", int limit = 20);

    // ========== PLAYBACK ==========
    void playTrack(const QString &trackId);
    void playTrackUri(const QString &uri);
    void playTrackInContext(const QString &trackId, const QVariantList &trackList, int index);
    void playContext(const QString &contextUri, int offset = 0);
    void play();
    void pause();
    void resume();
    void stop();
    void next();
    void previous();
    void seekTo(qint64 positionMs);
    void toggleShuffle();
    void cycleRepeatMode();

    // ========== QUEUE ==========
    void addToQueue(const QVariantList &tracks);
    void clearQueue();
    void removeFromQueue(int index);

    // ========== BROWSING ==========
    void getAlbum(const QString &albumId);
    void getArtist(const QString &artistId);
    void getPlaylist(const QString &playlistId);
    void getFavorites();
    void getPlaylists();

    // ========== FAVORITES ==========
    void addFavorite(const QString &trackId);
    void removeFavorite(const QString &trackId);

    // ========== DEVICE ==========
    void findDevice();

signals:
    void connectionChanged();
    void authStatusChanged();
    void loadingChanged();
    void statusMessageChanged();
    void loginUrlChanged();
    void playStateChanged();
    void trackChanged();
    void positionChanged();
    void durationChanged();
    void queueChanged();
    void queuePositionChanged();
    void shuffleChanged();
    void repeatModeChanged();
    void searchResultsChanged();

    // Data signals
    void albumReceived(const QVariantMap &album, const QVariantList &tracks);
    void artistReceived(const QVariantMap &artist, const QVariantList &topTracks, const QVariantList &albums);
    void playlistReceived(const QVariantMap &playlist, const QVariantList &tracks);
    void favoritesReceived(const QVariantList &tracks);
    void playlistsReceived(const QVariantList &playlists);
    void favoriteAdded(const QString &trackId);
    void favoriteRemoved(const QString &trackId);
    void error(const QString &message);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QLocalSocket::LocalSocketError socketError);
    void onSocketReadyRead();
    void onReconnectTimer();
    void onServiceStarted();
    void onServiceError(QProcess::ProcessError error);
    void onPositionTimer();
    void onPlaybackPollTimer();

private:
    void sendCommand(const QJsonObject &cmd);
    void handleResponse(const QJsonObject &response);
    void setStatusMessage(const QString &msg);
    void setLoading(bool loading);
    void setTrackFromQueue(int index);
    void playQueueItem(int index);

    // Socket
    QLocalSocket *m_socket;
    QByteArray m_readBuffer;

    // Service process
    QProcess *m_serviceProcess;

    // Reconnect
    QTimer *m_reconnectTimer;
    int m_reconnectAttempts;

    // Auth check polling
    QTimer *m_authCheckTimer;

    // Position interpolation
    QTimer *m_positionTimer;
    qint64 m_lastPollPosition;
    qint64 m_lastPollTime;

    // Playback state polling
    QTimer *m_playbackPollTimer;

    // State
    bool m_isConnected;
    bool m_isLoggedIn;
    QString m_userName;
    bool m_isLoading;
    QString m_statusMessage;

    // Auth flow
    QString m_loginUrl;

    // Now playing
    bool m_isPlaying;
    QString m_currentTrackUri;
    QString m_trackTitle;
    QString m_artist;
    QString m_album;
    QString m_albumArtUrl;
    int m_trackDuration;
    qint64 m_position;
    qint64 m_duration;
    bool m_isExplicit;

    // Queue
    QVariantList m_queue;
    int m_queuePosition;
    bool m_shuffleEnabled;
    int m_repeatMode; // 0=off, 1=context, 2=track

    // Search
    QVariantList m_searchResults;

    static constexpr const char* SOCKET_PATH = "/tmp/headunit_spotify.sock";
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int RECONNECT_INTERVAL_MS = 2000;
    static constexpr int AUTH_CHECK_INTERVAL_MS = 3000;
    static constexpr int POSITION_UPDATE_MS = 250;
    static constexpr int PLAYBACK_POLL_MS = 1000;
};

#endif // SPOTIFYCLIENT_H
