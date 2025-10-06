#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Platform-specific Bluetooth includes
#ifndef Q_OS_WIN
#include <QBluetoothAddress>
#include <QBluetoothSocket>
#include <QBluetoothServiceInfo>
#endif

/**
 * MediaController - Handles Bluetooth Music Playback Control
 *
 * This class implements A2DP (audio streaming) and AVRCP (remote control)
 * to control music playback from a connected phone.
 *
 * Key Features:
 * - Playback control (play, pause, stop, next, previous)
 * - Volume control
 * - Track seeking
 * - Metadata retrieval (title, artist, album, artwork)
 * - Playlist browsing (if supported by phone)
 * - Shuffle/Repeat modes
 * - Real-time position tracking
 *
 * Bluetooth Profiles Used:
 * - A2DP: Advanced Audio Distribution Profile (audio streaming)
 * - AVRCP: Audio/Video Remote Control Profile (metadata & control)
 */
class MediaController : public QObject
{
    Q_OBJECT

    // ========== PROPERTIES ==========

    /**
     * Connection Status
     * True when phone is connected and ready for music control
     */
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)

    /**
     * Playback State
     * True when music is currently playing
     */
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY playStateChanged)

    /**
     * Current Track Information
     */
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY trackChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY trackChanged)
    Q_PROPERTY(QString album READ album NOTIFY trackChanged)
    Q_PROPERTY(QString genre READ genre NOTIFY trackChanged)

    /**
     * Album Artwork
     * URL or local path to album cover image
     */
    Q_PROPERTY(QUrl albumArtUrl READ albumArtUrl NOTIFY albumArtChanged)

    /**
     * Track Timing
     * Position and duration in milliseconds
     */
    Q_PROPERTY(qint64 trackPosition READ trackPosition NOTIFY positionChanged)
    Q_PROPERTY(qint64 trackDuration READ trackDuration NOTIFY durationChanged)

    /**
     * Volume Control
     * Range: 0-100
     */
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)

    /**
     * Playback Modes
     */
    Q_PROPERTY(RepeatMode repeatMode READ repeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled NOTIFY shuffleChanged)

    /**
     * Currently Playing App
     * e.g., "Spotify", "Apple Music", "YouTube Music"
     */
    Q_PROPERTY(QString activeApp READ activeApp NOTIFY activeAppChanged)

    /**
     * Audio Source
     * "phone" = phone's music, "tidal" = local Tidal app, "radio" = FM/AM
     */
    Q_PROPERTY(QString audioSource READ audioSource NOTIFY audioSourceChanged)

    /**
     * Status Message
     * Human-readable status for debugging/display
     */
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    /**
     * Repeat Modes
     * - Off: Play through playlist once
     * - All: Repeat entire playlist
     * - One: Repeat current track
     */
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

    /**
     * Connect to phone's music service
     * @param deviceAddress: Bluetooth address of phone (e.g., "XX:XX:XX:XX:XX:XX")
     */
    void connectToDevice(const QString &deviceAddress);

    /**
     * Disconnect from music service
     */
    void disconnect();

    // ========== PLAYBACK CONTROLS ==========

    /**
     * Start or resume playback
     */
    void play();

    /**
     * Pause playback
     */
    void pause();

    /**
     * Stop playback completely
     * (Pause keeps position, Stop resets to beginning)
     */
    void stop();

    /**
     * Toggle between play and pause
     */
    void togglePlayPause();

    /**
     * Skip to next track
     */
    void next();

    /**
     * Skip to previous track
     * If >3 seconds into song, restarts current track
     */
    void previous();

    /**
     * Seek to specific position in track
     * @param positionMs: Position in milliseconds
     */
    void seekTo(qint64 positionMs);

    /**
     * Skip forward by specified time
     * @param seconds: Amount to skip (default: 10 seconds)
     */
    void skipForward(int seconds = 10);

    /**
     * Skip backward by specified time
     * @param seconds: Amount to skip (default: 10 seconds)
     */
    void skipBackward(int seconds = 10);

    // ========== VOLUME CONTROL ==========

    /**
     * Set volume level
     * @param level: Volume level 0-100
     */
    void setVolume(int level);

    /**
     * Increase volume by step
     * @param step: Amount to increase (default: 5)
     */
    void volumeUp(int step = 5);

    /**
     * Decrease volume by step
     * @param step: Amount to decrease (default: 5)
     */
    void volumeDown(int step = 5);

    /**
     * Mute/unmute audio
     */
    void toggleMute();

    // ========== PLAYBACK MODES ==========

    /**
     * Set repeat mode
     * @param mode: RepeatOff, RepeatAll, or RepeatOne
     */
    void setRepeatMode(RepeatMode mode);

    /**
     * Cycle through repeat modes
     * Off → All → One → Off
     */
    void cycleRepeatMode();

    /**
     * Enable or disable shuffle
     * @param enabled: True to enable shuffle
     */
    void setShuffle(bool enabled);

    /**
     * Toggle shuffle on/off
     */
    void toggleShuffle();

    // ========== AUDIO SOURCE SWITCHING ==========

    /**
     * Switch audio source
     * @param source: "phone", "tidal", "radio", "aux", "usb"
     */
    void setAudioSource(const QString &source);

    // ========== LIBRARY BROWSING ==========
    // Note: These require advanced AVRCP features (may not work on all phones)

    /**
     * Request list of playlists from phone
     */
    void requestPlaylists();

    /**
     * Request list of artists from phone
     */
    void requestArtists();

    /**
     * Request list of albums from phone
     */
    void requestAlbums();

    /**
     * Play specific playlist by ID
     * @param playlistId: ID from requestPlaylists()
     */
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

    /**
     * Emitted when an error occurs
     * @param message: Error description
     */
    void error(const QString &message);

    /**
     * Emitted when playlists are received
     * @param playlists: List of playlist objects
     */
    void playlistsReceived(const QVariantList &playlists);

    /**
     * Emitted when artists are received
     * @param artists: List of artist names
     */
    void artistsReceived(const QVariantList &artists);

    /**
     * Emitted when albums are received
     * @param albums: List of album objects
     */
    void albumsReceived(const QVariantList &albums);

private slots:
    // ========== INTERNAL SLOTS ==========

    /**
     * Update track position periodically
     * Called by m_positionTimer every second while playing
     */
    void updatePosition();

    /**
     * Handle artwork download completion
     */
    void onAlbumArtDownloaded();

#ifndef Q_OS_WIN
    /**
     * Handle Bluetooth socket events
     */
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError();
    void onSocketReadyRead();
#endif

private:
    // ========== HELPER METHODS ==========

    /**
     * Set status message and emit signal
     */
    void setStatusMessage(const QString &msg);

    /**
     * Send AVRCP command to phone
     * @param command: AVRCP command string
     */
    void sendAvrcpCommand(const QString &command);

    /**
     * Parse AVRCP response
     * @param response: Response data from phone
     */
    void parseAvrcpResponse(const QString &response);

    /**
     * Download album artwork from URL
     * @param url: Image URL from metadata
     */
    void downloadAlbumArt(const QUrl &url);

    /**
     * Generate mock music data for testing (Windows only)
     */
    void generateMockMusic();

    /**
     * Simulate track change for testing
     */
    void simulateTrackChange();

    // ========== MEMBER VARIABLES ==========

    // Connection state
    bool m_isConnected;
    QString m_deviceAddress;

    // Playback state
    bool m_isPlaying;
    RepeatMode m_repeatMode;
    bool m_shuffleEnabled;

    // Track information
    QString m_trackTitle;
    QString m_artist;
    QString m_album;
    QString m_genre;
    QUrl m_albumArtUrl;
    QImage m_albumArtImage;

    // Track timing
    qint64 m_trackPosition;    // Current position in milliseconds
    qint64 m_trackDuration;    // Total duration in milliseconds

    // Volume
    int m_volume;
    int m_savedVolume;         // For mute/unmute
    bool m_isMuted;

    // App information
    QString m_activeApp;       // "Spotify", "Apple Music", etc.
    QString m_audioSource;     // "phone", "tidal", "radio", etc.

    // Status
    QString m_statusMessage;

    // Timers
    QTimer *m_positionTimer;   // Updates track position every second
    QTimer *m_mockTimer;       // Generates mock data (testing only)

    // Network
    QNetworkAccessManager *m_networkManager;  // For downloading album art

#ifndef Q_OS_WIN
    // Bluetooth (real mode)
    QBluetoothAddress m_bluetoothAddress;
    QBluetoothSocket *m_socket;
    QBluetoothServiceInfo m_avrcpService;
#endif

    // Mock mode flag
    bool m_mockMode;
};

#endif // MEDIACONTROLLER_H
