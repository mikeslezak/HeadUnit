#ifndef TOOLEXECUTOR_H
#define TOOLEXECUTOR_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantMap>
#include <QMap>
#include <QTimer>

class ContactManager;
class MessageManager;
class BluetoothManager;
class PlacesSearchManager;
class TidalClient;
class SpotifyClient;
class MediaController;
class CopilotMonitor;

/**
 * ToolExecutor - Executes Claude API tool calls by dispatching to managers
 *
 * Provides tool definitions (JSON schemas) that are sent with every Claude API
 * request, and executes tool calls that Claude returns. Some tools are sync
 * (navigate, call, playback control) and some are async (places search, music search).
 *
 * For async tools, emits toolCompleted() when the result is ready.
 * ClaudeClient manages the tool loop: tool_use → execute → tool_result → next API call.
 */
class ToolExecutor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool awaitingConfirmation READ awaitingConfirmation NOTIFY awaitingConfirmationChanged)
    Q_PROPERTY(QString pendingAction READ pendingAction NOTIFY pendingActionChanged)

public:
    explicit ToolExecutor(QObject *parent = nullptr);

    /**
     * Returns the full tool definitions array for the Claude API request.
     */
    static QJsonArray toolDefinitions();

    /**
     * Execute a tool call. Returns a result immediately for sync tools,
     * or an empty object for async tools (result arrives via toolCompleted signal).
     */
    QJsonObject executeTool(const QString &toolUseId, const QString &toolName, const QJsonObject &input);

    // Property getters
    bool awaitingConfirmation() const { return m_awaitingConfirmation; }
    QString pendingAction() const { return m_pendingAction; }

    // Dependency injection
    void setContactManager(ContactManager *mgr) { m_contactManager = mgr; }
    void setMessageManager(MessageManager *mgr) { m_messageManager = mgr; }
    void setBluetoothManager(BluetoothManager *mgr) { m_bluetoothManager = mgr; }
    void setPlacesSearchManager(PlacesSearchManager *mgr);
    void setTidalClient(TidalClient *client);
    void setSpotifyClient(SpotifyClient *client);
    void setMediaController(MediaController *mgr) { m_mediaController = mgr; }
    void setCopilotMonitor(CopilotMonitor *mgr) { m_copilotMonitor = mgr; }

public slots:
    void confirmAction();
    void cancelAction();

signals:
    void awaitingConfirmationChanged();
    void pendingActionChanged();

    /** Emitted when an async tool finishes. ClaudeClient uses this to submit tool_result. */
    void toolCompleted(const QString &toolUseId, const QJsonObject &result);

    /** Navigation requested — VoicePipeline triggers geocoding + map route */
    void navigationStarted(const QString &destination);

    /** Add a stop along the active route (waypoint) */
    void routeStopRequested(const QString &destination);

    /** Route cancelled by voice command */
    void routeCancelled();

    /** Claude wants the mic to stay open for a follow-up reply */
    void followUpExpected();

    /** SMS confirmation dialog should be shown */
    void confirmationRequested(const QString &action, const QString &details);

private:
    // Tool handlers (return result object; async tools return empty and signal later)
    QJsonObject handleNavigate(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleAddStop(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleSearchPlaces(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleCallContact(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleSendMessage(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleReadMessages(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleQuietMode(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handlePlayMusic(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleControlPlayback(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleSetFollowUp(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleMusicInfo(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleAddFavorite(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleHangupCall(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleAnswerCall(const QString &toolUseId, const QJsonObject &input);
    QJsonObject handleCancelRoute(const QString &toolUseId, const QJsonObject &input);

    // Helpers
    QString findContactPhoneNumber(const QString &contactName);

    // Managers
    ContactManager *m_contactManager = nullptr;
    MessageManager *m_messageManager = nullptr;
    BluetoothManager *m_bluetoothManager = nullptr;
    PlacesSearchManager *m_placesSearchManager = nullptr;
    TidalClient *m_tidalClient = nullptr;
    SpotifyClient *m_spotifyClient = nullptr;
    MediaController *m_mediaController = nullptr;
    CopilotMonitor *m_copilotMonitor = nullptr;

    // SMS confirmation state
    bool m_awaitingConfirmation = false;
    QString m_pendingAction;
    QVariantMap m_pendingCommand;
    QString m_pendingToolUseId;

    // Async tool tracking
    QString m_pendingSearchToolId;
    QString m_pendingMusicToolId;
    QString m_pendingMusicType;  // "tracks", "albums", "artists" — what kind of search is pending
    QString m_pendingMusicSource; // "tidal" or "spotify"
    int m_musicGeneration = 0;   // Incremented per voice music request — prevents UI cross-fire
    int m_expectedAlbumId = -1;  // Tidal album ID we're waiting for (prevents UI album browse cross-fire)
    int m_expectedArtistId = -1; // Tidal artist ID we're waiting for
    QString m_expectedSpotifyAlbumId;  // Spotify album ID
    QString m_expectedSpotifyArtistId; // Spotify artist ID

public:
    // Called by ClaudeClient when canceling/timing out to clear stale pending state
    Q_INVOKABLE void clearPendingTools();
};

#endif // TOOLEXECUTOR_H
