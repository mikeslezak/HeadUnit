#ifndef TIDALCLIENT_H
#define TIDALCLIENT_H

#include <QObject>
#include <QString>
#include <QLocalSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QProcess>

#include <gst/gst.h>

/**
 * TidalClient - Bridge between QML and the Python Tidal service
 *
 * Communicates with tidal_service.py over a Unix domain socket
 * using newline-delimited JSON. Handles audio playback via GStreamer playbin.
 *
 * The Python service handles:
 *   - OAuth device authorization flow
 *   - Tidal API calls (search, catalog browsing)
 *   - Stream URL retrieval
 *
 * This C++ class handles:
 *   - Socket connection management
 *   - JSON protocol encoding/decoding
 *   - GStreamer audio playback (playbin)
 *   - Queue management (next/previous/shuffle/repeat)
 *   - Exposing everything to QML
 */
class TidalClient : public QObject
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
    Q_PROPERTY(QString loginCode READ loginCode NOTIFY loginUrlChanged)

    // ========== NOW PLAYING ==========
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playStateChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString albumArtUrl READ albumArtUrl NOTIFY trackChanged)
    Q_PROPERTY(int trackDuration READ trackDuration NOTIFY trackChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QString audioQuality READ audioQuality NOTIFY trackChanged)

    // ========== QUEUE ==========
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int queuePosition READ queuePosition NOTIFY queuePositionChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled NOTIFY shuffleChanged)
    Q_PROPERTY(int repeatMode READ repeatMode NOTIFY repeatModeChanged)

    // ========== SEARCH ==========
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)

public:
    explicit TidalClient(QObject *parent = nullptr);
    ~TidalClient();

    // Property getters
    bool isConnected() const { return m_isConnected; }
    bool isLoggedIn() const { return m_isLoggedIn; }
    QString userName() const { return m_userName; }
    bool isLoading() const { return m_isLoading; }
    QString statusMessage() const { return m_statusMessage; }
    QString loginUrl() const { return m_loginUrl; }
    QString loginCode() const { return m_loginCode; }
    bool isPlaying() const { return m_isPlaying; }
    QString trackTitle() const { return m_trackTitle; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString albumArtUrl() const { return m_albumArtUrl; }
    int trackDuration() const { return m_trackDuration; }
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    QString audioQuality() const { return m_audioQuality; }
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
    void playTrack(int trackId);
    void playTrackInContext(int trackId, const QVariantList &trackList, int index);
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
    void getAlbum(int albumId);
    void getArtist(int artistId);
    void getPlaylist(const QString &playlistId);
    void getFavorites();
    void getHome();

    // ========== FAVORITES ==========
    void addFavorite(int trackId);
    void removeFavorite(int trackId);

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
    void homeReceived(const QVariantList &mixes);
    void streamReady(const QString &url, const QString &codec, const QString &quality);
    void favoriteAdded(int trackId);
    void favoriteRemoved(int trackId);
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

private:
    void sendCommand(const QJsonObject &cmd);
    void handleResponse(const QJsonObject &response);
    void setStatusMessage(const QString &msg);
    void setLoading(bool loading);

    // GStreamer
    void initGStreamer();
    void destroyGStreamer();
    void playUrl(const QString &url);
    void setTrackFromQueue(int index);
    void requestStreamForQueueItem(int index);
    static gboolean onBusMessage(GstBus *bus, GstMessage *msg, gpointer data);

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

    // Position polling
    QTimer *m_positionTimer;

    // State
    bool m_isConnected;
    bool m_isLoggedIn;
    QString m_userName;
    bool m_isLoading;
    QString m_statusMessage;

    // Auth flow
    QString m_loginUrl;
    QString m_loginCode;

    // Now playing
    bool m_isPlaying;
    QString m_trackTitle;
    QString m_artist;
    QString m_album;
    QString m_albumArtUrl;
    int m_trackDuration;
    qint64 m_position;
    qint64 m_duration;
    QString m_audioQuality;
    QString m_streamUrl;

    // Queue
    QVariantList m_queue;
    int m_queuePosition;
    bool m_shuffleEnabled;
    int m_repeatMode; // 0=off, 1=all, 2=one
    QList<int> m_shuffleOrder;

    // Search
    QVariantList m_searchResults;

    // GStreamer
    GstElement *m_pipeline;
    guint m_busWatchId;

    // Pending stream request (track ID we're waiting on)
    int m_pendingTrackId;

    static constexpr const char* SOCKET_PATH = "/tmp/headunit_tidal.sock";
    static constexpr int MAX_RECONNECT_ATTEMPTS = 10;
    static constexpr int RECONNECT_INTERVAL_MS = 2000;
    static constexpr int AUTH_CHECK_INTERVAL_MS = 3000;
    static constexpr int POSITION_UPDATE_MS = 250;
};

#endif // TIDALCLIENT_H
