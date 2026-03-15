#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Platform-specific includes
#ifndef Q_OS_WIN
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusObjectPath>
#include <QDBusMessage>
#include <QProcess>
#endif

/**
 * MediaController - Handles Bluetooth Music Playback Control
 *
 * This class implements A2DP (audio streaming) and AVRCP (remote control)
 * to control music playback from a connected phone.
 *
 * Key Features:
 * - Playback control (play, pause, stop, next, previous)
 * - Track metadata retrieval (title, artist, album, duration)
 * - Real-time position tracking
 * - Shuffle/Repeat modes
 *
 * Bluetooth Profiles Used:
 * - A2DP: Advanced Audio Distribution Profile (audio streaming)
 * - AVRCP: Audio/Video Remote Control Profile (metadata & control)
 *
 * NOTE: Volume control intentionally excluded for bit-perfect audio output
 */
class MediaController : public QObject
{
    Q_OBJECT

    // ========== PROPERTIES ==========

    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playStateChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString genre READ genre NOTIFY trackChanged)
    Q_PROPERTY(QUrl albumArtUrl READ albumArtUrl NOTIFY albumArtChanged)
    Q_PROPERTY(qint64 trackPosition READ trackPosition NOTIFY positionChanged)
    Q_PROPERTY(qint64 trackDuration READ trackDuration NOTIFY durationChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(RepeatMode repeatMode READ repeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled NOTIFY shuffleChanged)
    Q_PROPERTY(QString activeApp READ activeApp NOTIFY activeAppChanged)
    Q_PROPERTY(QString audioSource READ audioSource NOTIFY audioSourceChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    enum RepeatMode {
        RepeatOff = 0,
        RepeatAll = 1,
        RepeatOne = 2
    };
    Q_ENUM(RepeatMode)

    explicit MediaController(QObject *parent = nullptr);
    ~MediaController();

    // ========== PROPERTY GETTERS ==========

    bool isConnected() const { return m_isConnected; }
    bool isPlaying() const { return m_isPlaying; }
    QString trackTitle() const { return m_trackTitle; }
    QString artist() const { return m_artist; }
    QString album() const { return m_album; }
    QString genre() const { return m_genre; }
    QUrl albumArtUrl() const { return m_albumArtUrl; }
    qint64 trackPosition() const { return m_trackPosition; }
    qint64 trackDuration() const { return m_trackDuration; }
    int volume() const { return m_volume; }
    RepeatMode repeatMode() const { return m_repeatMode; }
    bool shuffleEnabled() const { return m_shuffleEnabled; }
    QString activeApp() const { return m_activeApp; }
    QString audioSource() const { return m_audioSource; }
    QString statusMessage() const { return m_statusMessage; }

public slots:
    // ========== CONNECTION MANAGEMENT ==========
    void connectToDevice(const QString &deviceAddress);
    void disconnect();

    // ========== PLAYBACK CONTROLS ==========
    void play();
    void pause();
    void stop();
    void togglePlayPause();
    void next();
    void previous();
    void seekTo(qint64 positionMs);
    void skipForward(int seconds = 10);
    void skipBackward(int seconds = 10);

    // ========== VOLUME CONTROL - DISABLED ==========
    void setVolume(int level);
    void volumeUp(int step = 5);
    void volumeDown(int step = 5);
    void toggleMute();

    // ========== PLAYBACK MODES ==========
    void setRepeatMode(RepeatMode mode);
    void cycleRepeatMode();
    void setShuffle(bool enabled);
    void toggleShuffle();

    // ========== AUDIO SOURCE SWITCHING ==========
    void setAudioSource(const QString &source);

    // ========== LIBRARY BROWSING ==========
    void requestPlaylists();
    void requestArtists();
    void requestAlbums();
    void playPlaylist(const QString &playlistId);

signals:
    // ========== SIGNALS ==========
    void connectionChanged();
    void playStateChanged();
    void trackChanged();
    void albumArtChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void repeatModeChanged();
    void shuffleChanged();
    void activeAppChanged();
    void audioSourceChanged();
    void statusMessageChanged();
    void error(const QString &message);
    void playlistsReceived(const QVariantList &playlists);
    void artistsReceived(const QVariantList &artists);
    void albumsReceived(const QVariantList &albums);

private slots:
    // ========== INTERNAL SLOTS ==========
    void updatePosition();
    void onAlbumArtDownloaded();

#ifndef Q_OS_WIN
    void onPropertiesChanged(const QString &interface,
                             const QVariantMap &changedProperties,
                             const QStringList &invalidatedProperties);
    void onInterfacesAdded(const QDBusObjectPath &path, const QVariantMap &interfaces);
    void setupAudioRouting();
    void teardownAudioRouting();
#endif

private:
    // ========== HELPER METHODS ==========
    void setStatusMessage(const QString &msg);
    void sendAvrcpCommand(const QString &command);
    void parseAvrcpResponse(const QString &response);
    
#ifndef Q_OS_WIN
    QString findMediaPlayerPath(const QString &devicePath);
    void setupPropertyMonitoring(const QString &path);
    QString addressToPath(const QString &address);
    QVariantMap extractTrackMetadata(const QVariant &trackVariant);
#endif

    void downloadAlbumArt(const QUrl &url);
    void generateMockMusic();
    void simulateTrackChange();

    // ========== MEMBER VARIABLES ==========
    bool m_isConnected;
    QString m_deviceAddress;
    bool m_isPlaying;
    RepeatMode m_repeatMode;
    bool m_shuffleEnabled;
    QString m_trackTitle;
    QString m_artist;
    QString m_album;
    QString m_genre;
    QUrl m_albumArtUrl;
    QImage m_albumArtImage;
    qint64 m_trackPosition;
    qint64 m_trackDuration;
    int m_volume;
    int m_savedVolume;
    bool m_isMuted;
    QString m_activeApp;
    QString m_audioSource;
    QString m_statusMessage;
    QTimer *m_positionTimer;
    QTimer *m_mockTimer;
    QNetworkAccessManager *m_networkManager;

#ifndef Q_OS_WIN
    QDBusInterface *m_deviceInterface;
    QDBusInterface *m_mediaPlayerInterface;
    QDBusInterface *m_mediaControlInterface;
    QString m_mediaPlayerPath;
    QString m_monitoredPath;  // Track which path we're monitoring to prevent signal accumulation
    int m_pulseLoopbackModule;
#endif

    bool m_mockMode;
};

#endif // MEDIACONTROLLER_H
