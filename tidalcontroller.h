#ifndef TIDALCONTROLLER_H
#define TIDALCONTROLLER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QDebug>
#include <QVariantMap>
#include <QVariantList>

class TidalController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playStateChanged)
    Q_PROPERTY(QString currentTrack READ currentTrack NOTIFY trackChanged)
    Q_PROPERTY(QString currentArtist READ currentArtist NOTIFY trackChanged)
    Q_PROPERTY(QString currentAlbum READ currentAlbum NOTIFY trackChanged)
    Q_PROPERTY(QString albumArtUrl READ albumArtUrl NOTIFY trackChanged)
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int trackPosition READ trackPosition NOTIFY trackPositionChanged)
    Q_PROPERTY(int trackDuration READ trackDuration NOTIFY trackDurationChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(bool isSearching READ isSearching NOTIFY isSearchingChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY playlistsChanged)
    Q_PROPERTY(QVariantList recentlyPlayed READ recentlyPlayed NOTIFY recentlyPlayedChanged)
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY downloadsChanged)
    Q_PROPERTY(QVariantList favorites READ favorites NOTIFY favoritesChanged)

public:
    explicit TidalController(QObject *parent = nullptr);
    ~TidalController();

    bool isPlaying() const { return m_isPlaying; }
    QString currentTrack() const { return m_currentTrack; }
    QString currentArtist() const { return m_currentArtist; }
    QString currentAlbum() const { return m_currentAlbum; }
    QString albumArtUrl() const { return m_albumArtUrl; }
    bool isConnected() const { return m_isConnected; }
    QString statusMessage() const { return m_statusMessage; }
    int trackPosition() const { return m_trackPosition; }
    int trackDuration() const { return m_trackDuration; }
    QVariantList searchResults() const { return m_searchResults; }
    bool isSearching() const { return m_isSearching; }
    QVariantList queue() const { return m_queue; }
    QVariantList playlists() const { return m_playlists; }
    QVariantList recentlyPlayed() const { return m_recentlyPlayed; }
    QVariantList downloads() const { return m_downloads; }
    QVariantList favorites() const { return m_favorites; }

public slots:
    void play();
    void pause();
    void togglePlayPause();
    void next();
    void previous();
    void seekTo(int positionMs);

    // Search
    void search(const QString &query);
    void playTrack(const QString &trackId);

    // Queue management
    void addToQueue(const QVariantMap &track);
    void removeFromQueue(int index);
    void clearQueue();
    void playFromQueue(int index);

    // Library
    void loadPlaylists();
    void loadPlaylist(const QString &playlistId);
    void loadRecentlyPlayed();
    void loadFavorites();
    void loadDownloads();

    // Favorites management
    void addToFavorites(const QString &trackId);
    void removeFromFavorites(const QString &trackId);
    bool isFavorite(const QString &trackId);

    // Downloads management
    void downloadTrack(const QString &trackId);
    void removeDownload(const QString &trackId);
    bool isDownloaded(const QString &trackId);

    // Connection management
    void checkConnection();
    void startTidalApp();
    void stopTidalApp();
    void syncWithAndroidApp();

    // Development/testing
    void simulatePlay();
    void simulatePause();
    void simulateTrackChange(const QString &trackName, const QString &artist = "", const QString &album = "");

signals:
    void playStateChanged();
    void trackChanged();
    void connectionChanged();
    void statusMessageChanged();
    void trackPositionChanged();
    void trackDurationChanged();
    void searchResultsChanged();
    void isSearchingChanged();
    void queueChanged();
    void playlistsChanged();
    void recentlyPlayedChanged();
    void downloadsChanged();
    void favoritesChanged();
    void error(const QString &message);

private slots:
    void onAdbCommandFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void pollMediaSession();
    void onAdbError(QProcess::ProcessError error);
    void updateTrackPosition();

private:
    void sendAdbCommand(const QString &command, const QStringList &args = {});
    void parseMediaSessionState(const QString &output);
    void parseDownloadsFromDb(const QString &output);
    void parseFavoritesFromDb(const QString &output);
    void parsePlaylistsFromDb(const QString &output);
    void setStatusMessage(const QString &msg);
    void generateMockSearchResults(const QString &query);
    void loadMockTrack(const QVariantMap &track);
    void generateMockPlaylists();
    void generateMockRecentlyPlayed();
    void generateMockDownloads();
    void generateMockFavorites();
    void addToRecentlyPlayed(const QVariantMap &track);
    void advanceQueue();
    void queryAndroidAppDatabase(const QString &query, const QString &purpose);

    QProcess *m_adbProcess;
    QTimer *m_pollTimer;
    QTimer *m_progressTimer;

    bool m_isPlaying;
    QString m_currentTrack;
    QString m_currentArtist;
    QString m_currentAlbum;
    QString m_albumArtUrl;
    bool m_isConnected;
    QString m_statusMessage;
    int m_trackPosition;
    int m_trackDuration;
    QVariantList m_searchResults;
    bool m_isSearching;
    QVariantList m_queue;
    QVariantList m_playlists;
    QVariantList m_recentlyPlayed;
    QVariantList m_downloads;
    QVariantList m_favorites;
    QSet<QString> m_favoriteIds;
    QSet<QString> m_downloadedIds;

    QString m_pendingQueryPurpose;

    static const QString TIDAL_PACKAGE;
    static const QString TIDAL_ACTIVITY;

#ifdef Q_OS_WIN
    bool m_mockMode;
#endif
};

#endif // TIDALCONTROLLER_H
