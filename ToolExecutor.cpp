#include "ToolExecutor.h"
#include "ContactManager.h"
#include "MessageManager.h"
#include "BluetoothManager.h"
#include "PlacesSearchManager.h"
#include "TidalClient.h"
#include "SpotifyClient.h"
#include "MediaController.h"
#include "CopilotMonitor.h"
#include <QDebug>
#include <QMetaObject>
#include <QJsonDocument>

ToolExecutor::ToolExecutor(QObject *parent)
    : QObject(parent)
{
    qDebug() << "ToolExecutor: Initialized";
}

// ========================================================================
// TOOL DEFINITIONS — sent to Claude API with every request
// ========================================================================

QJsonArray ToolExecutor::toolDefinitions()
{
    QJsonArray tools;

    // --- navigate ---
    {
        QJsonObject tool;
        tool["name"] = "navigate";
        tool["description"] = "Start navigation to a destination. The system will geocode the text and calculate a route. Always use this when the user wants to go somewhere.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject dest; dest["type"] = "string"; dest["description"] = "Place name, address, or business name to navigate to";
        props["destination"] = dest;
        schema["properties"] = props;
        schema["required"] = QJsonArray({"destination"});
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- add_stop ---
    {
        QJsonObject tool;
        tool["name"] = "add_stop";
        tool["description"] = "Add a stop/waypoint along the active navigation route. The route is recalculated to pass through this stop before continuing to the final destination. Use this instead of navigate when the user wants to stop somewhere along an active route (e.g. 'let's stop there', 'add that to the route', 'pull over at that gas station').";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject dest; dest["type"] = "string"; dest["description"] = "Place name, address, or business name to add as a stop along the route";
        props["destination"] = dest;
        schema["properties"] = props;
        schema["required"] = QJsonArray({"destination"});
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- search_places ---
    {
        QJsonObject tool;
        tool["name"] = "search_places";
        tool["description"] = "Search for places or points of interest (restaurants, gas stations, hotels, etc). Returns names, distances, and ratings. Use along_route=true when the user wants food/stops along their active route. After results, present them conversationally and ask which one.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject query; query["type"] = "string"; query["description"] = "Search query (e.g. 'tacos', 'gas station', 'coffee')";
        QJsonObject category; category["type"] = "string"; category["description"] = "Optional category filter (e.g. 'restaurant', 'gas_station', 'hotel')";
        QJsonObject alongRoute; alongRoute["type"] = "boolean"; alongRoute["description"] = "Search along the active navigation route instead of just nearby. Use when user says 'along the way', 'on the route', 'coming up', or wants stops during a trip.";
        QJsonObject near; near["type"] = "string"; near["description"] = "Search near a specific city or location instead of current GPS (e.g. 'Red Deer', 'Banff'). Geocodes the location first, then searches nearby.";
        props["query"] = query;
        props["category"] = category;
        props["along_route"] = alongRoute;
        props["near"] = near;
        schema["properties"] = props;
        schema["required"] = QJsonArray({"query"});
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- call_contact ---
    {
        QJsonObject tool;
        tool["name"] = "call_contact";
        tool["description"] = "Make a phone call to a contact or phone number via Bluetooth.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject name; name["type"] = "string"; name["description"] = "Contact name to call";
        QJsonObject number; number["type"] = "string"; number["description"] = "Phone number to dial (if no contact name)";
        props["contact_name"] = name;
        props["phone_number"] = number;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- send_message ---
    {
        QJsonObject tool;
        tool["name"] = "send_message";
        tool["description"] = "Send an SMS text message to a contact. The system will show a confirmation dialog before sending.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject name; name["type"] = "string"; name["description"] = "Contact name to message";
        QJsonObject body; body["type"] = "string"; body["description"] = "Message text to send";
        QJsonObject number; number["type"] = "string"; number["description"] = "Phone number (if no contact name)";
        props["contact_name"] = name;
        props["message_body"] = body;
        props["phone_number"] = number;
        schema["properties"] = props;
        schema["required"] = QJsonArray({"message_body"});
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- read_messages ---
    {
        QJsonObject tool;
        tool["name"] = "read_messages";
        tool["description"] = "Read recent text messages. Without a contact name, returns a summary of recent conversations. With a contact name, returns the last 10 messages in that thread.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject name; name["type"] = "string"; name["description"] = "Contact name to filter messages by (optional)";
        props["contact_name"] = name;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- play_music ---
    {
        QJsonObject tool;
        tool["name"] = "play_music";
        tool["description"] = "Search for and play music. Can play a specific song, an entire album, or an artist's top tracks. Uses Tidal or Spotify depending on which is logged in.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject query; query["type"] = "string"; query["description"] = "What to search for — song name, artist name, or album name";
        QJsonObject type; type["type"] = "string";
        type["description"] = "What to search for: 'tracks' for a specific song, 'albums' to play a full album, 'artists' to play an artist's top tracks";
        type["enum"] = QJsonArray({"tracks", "albums", "artists"});
        QJsonObject source; source["type"] = "string"; source["description"] = "Music source: 'tidal' or 'spotify'. Auto-detected if not specified.";
        props["query"] = query;
        props["type"] = type;
        props["source"] = source;
        schema["properties"] = props;
        schema["required"] = QJsonArray({"query"});
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- control_playback ---
    {
        QJsonObject tool;
        tool["name"] = "control_playback";
        tool["description"] = "Control music playback: play, pause, skip, previous, shuffle, repeat. Can also seek to a position in the current track.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject cmd; cmd["type"] = "string";
        cmd["description"] = "Playback command";
        cmd["enum"] = QJsonArray({"play", "pause", "next", "previous", "shuffle", "repeat"});
        QJsonObject seekMs; seekMs["type"] = "integer"; seekMs["description"] = "Seek to this position in milliseconds. Use instead of command for seeking. Example: 90000 for 1:30.";
        QJsonObject source; source["type"] = "string"; source["description"] = "Music source: 'tidal', 'spotify', 'bluetooth'. Auto-detected if not specified.";
        props["command"] = cmd;
        props["seek_ms"] = seekMs;
        props["source"] = source;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- music_info ---
    {
        QJsonObject tool;
        tool["name"] = "music_info";
        tool["description"] = "Get info about what's currently playing: track name, artist, album, playback state, position, and audio quality. Use when the user asks 'what's playing?', 'who sings this?', 'what song is this?'.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- add_favorite ---
    {
        QJsonObject tool;
        tool["name"] = "add_favorite";
        tool["description"] = "Save or unsave the currently playing track to the user's favorites/library. Use when the user says 'save this song', 'add to favorites', 'like this', or 'remove from favorites'.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject remove; remove["type"] = "boolean"; remove["description"] = "Set to true to remove from favorites instead of adding";
        props["remove"] = remove;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- hangup_call ---
    {
        QJsonObject tool;
        tool["name"] = "hangup_call";
        tool["description"] = "End the current phone call. Use when the user says 'hang up', 'end call', 'disconnect'.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- answer_call ---
    {
        QJsonObject tool;
        tool["name"] = "answer_call";
        tool["description"] = "Answer an incoming phone call. Use when the user says 'answer', 'pick up', 'accept the call'.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- quiet_mode ---
    {
        QJsonObject tool;
        tool["name"] = "quiet_mode";
        tool["description"] = "Toggle proactive copilot alerts on or off. Use when the user says 'be quiet', 'stop alerts', 'quiet mode', or similar.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        QJsonObject enabled; enabled["type"] = "boolean"; enabled["description"] = "true to silence alerts, false to re-enable";
        props["enabled"] = enabled;
        schema["properties"] = props;
        schema["required"] = QJsonArray({"enabled"});
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- set_follow_up ---
    {
        QJsonObject tool;
        tool["name"] = "set_follow_up";
        tool["description"] = "Keep the microphone open after your response so the user can reply without saying the wake word. Use this when you ask a question like 'want directions?' or 'which one?'.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    // --- cancel_route ---
    {
        QJsonObject tool;
        tool["name"] = "cancel_route";
        tool["description"] = "Cancel the active navigation route. Use when the user says 'cancel navigation', 'stop navigating', 'cancel the route', 'end navigation', etc.";
        QJsonObject schema;
        schema["type"] = "object";
        QJsonObject props;
        schema["properties"] = props;
        tool["input_schema"] = schema;
        tools.append(tool);
    }

    return tools;
}

// ========================================================================
// DEPENDENCY WIRING — connect async signals from managers
// ========================================================================

void ToolExecutor::setPlacesSearchManager(PlacesSearchManager *mgr)
{
    m_placesSearchManager = mgr;

    // When search completes, emit toolCompleted with the results
    connect(mgr, &PlacesSearchManager::searchCompleted, this, [this](const QString &results) {
        if (!m_pendingSearchToolId.isEmpty()) {
            QJsonObject result;
            result["status"] = "success";
            result["results"] = results;
            emit toolCompleted(m_pendingSearchToolId, result);
            m_pendingSearchToolId.clear();
        }
    });

    connect(mgr, &PlacesSearchManager::searchFailed, this, [this](const QString &error) {
        if (!m_pendingSearchToolId.isEmpty()) {
            QJsonObject result;
            result["status"] = "error";
            result["error"] = error;
            emit toolCompleted(m_pendingSearchToolId, result);
            m_pendingSearchToolId.clear();
        }
    });
}

void ToolExecutor::setTidalClient(TidalClient *client)
{
    m_tidalClient = client;

    // When Tidal search completes, handle based on search type
    // Guard with generation counter to prevent UI-initiated searches from cross-firing
    connect(client, &TidalClient::searchResultsChanged, this, [this]() {
        if (m_pendingMusicToolId.isEmpty() || !m_tidalClient || m_pendingMusicSource != "tidal") return;

        QVariantList results = m_tidalClient->searchResults();
        if (results.isEmpty()) {
            QJsonObject result;
            result["status"] = "no_results";
            result["message"] = QString("No %1 found on Tidal.").arg(m_pendingMusicType);
            emit toolCompleted(m_pendingMusicToolId, result);
            m_pendingMusicToolId.clear();
            return;
        }

        if (m_pendingMusicType == "albums") {
            // Search returned album results — fetch the album to get track list
            QVariantMap first = results.first().toMap();
            int albumId = first["id"].toInt();
            m_expectedAlbumId = albumId;
            qDebug() << "ToolExecutor: Tidal album found:" << first["title"].toString() << "- fetching tracks";
            m_tidalClient->getAlbum(albumId);
            // Result will come via albumReceived signal
        } else if (m_pendingMusicType == "artists") {
            // Search returned artist results — fetch artist top tracks
            QVariantMap first = results.first().toMap();
            int artistId = first["id"].toInt();
            m_expectedArtistId = artistId;
            qDebug() << "ToolExecutor: Tidal artist found:" << first["name"].toString() << "- fetching top tracks";
            m_tidalClient->getArtist(artistId);
            // Result will come via artistReceived signal
        } else {
            // Track search — auto-play first result
            QVariantMap first = results.first().toMap();
            int trackId = first["id"].toInt();
            m_tidalClient->playTrackInContext(trackId, results, 0);
            QJsonObject result;
            result["status"] = "playing";
            result["track"] = first["title"].toString();
            result["artist"] = first["artist"].toString();
            emit toolCompleted(m_pendingMusicToolId, result);
            m_pendingMusicToolId.clear();
        }
    });

    // When album data arrives, queue all tracks and play
    connect(client, &TidalClient::albumReceived, this, [this](const QVariantMap &album, const QVariantList &tracks) {
        if (m_pendingMusicToolId.isEmpty() || m_pendingMusicSource != "tidal") return;
        // Verify this is the album we requested, not a UI-initiated browse
        if (album["id"].toInt() != m_expectedAlbumId) return;

        QJsonObject result;
        if (tracks.isEmpty()) {
            result["status"] = "error";
            result["error"] = "Album has no playable tracks.";
        } else {
            int firstId = tracks.first().toMap()["id"].toInt();
            m_tidalClient->playTrackInContext(firstId, tracks, 0);
            result["status"] = "playing";
            result["album"] = album["title"].toString();
            result["artist"] = album["artist"].toString();
            result["track_count"] = tracks.size();
        }
        emit toolCompleted(m_pendingMusicToolId, result);
        m_pendingMusicToolId.clear();
    });

    // When artist data arrives, queue top tracks and play
    connect(client, &TidalClient::artistReceived, this, [this](const QVariantMap &artist, const QVariantList &topTracks, const QVariantList &) {
        if (m_pendingMusicToolId.isEmpty() || m_pendingMusicSource != "tidal") return;
        // Verify this is the artist we requested, not a UI-initiated browse
        if (artist["id"].toInt() != m_expectedArtistId) return;

        QJsonObject result;
        if (topTracks.isEmpty()) {
            result["status"] = "error";
            result["error"] = "No tracks found for this artist.";
        } else {
            int firstId = topTracks.first().toMap()["id"].toInt();
            m_tidalClient->playTrackInContext(firstId, topTracks, 0);
            result["status"] = "playing";
            result["artist"] = artist["name"].toString();
            result["track"] = topTracks.first().toMap()["title"].toString();
            result["track_count"] = topTracks.size();
        }
        emit toolCompleted(m_pendingMusicToolId, result);
        m_pendingMusicToolId.clear();
    });

    // Handle Tidal errors during pending music tool (prevents chain hang)
    connect(client, &TidalClient::error, this, [this](const QString &message) {
        if (m_pendingMusicToolId.isEmpty() || m_pendingMusicSource != "tidal") return;
        QJsonObject result;
        result["status"] = "error";
        result["error"] = "Tidal error: " + message;
        emit toolCompleted(m_pendingMusicToolId, result);
        m_pendingMusicToolId.clear();
    });
}

void ToolExecutor::setSpotifyClient(SpotifyClient *client)
{
    m_spotifyClient = client;

    // When Spotify search completes, handle based on search type
    connect(client, &SpotifyClient::searchResultsChanged, this, [this]() {
        if (m_pendingMusicToolId.isEmpty() || !m_spotifyClient || m_pendingMusicSource != "spotify") return;

        QVariantList results = m_spotifyClient->searchResults();
        if (results.isEmpty()) {
            QJsonObject result;
            result["status"] = "no_results";
            result["message"] = QString("No %1 found on Spotify.").arg(m_pendingMusicType);
            emit toolCompleted(m_pendingMusicToolId, result);
            m_pendingMusicToolId.clear();
            return;
        }

        if (m_pendingMusicType == "albums") {
            QVariantMap first = results.first().toMap();
            QString albumId = first["id"].toString();
            m_expectedSpotifyAlbumId = albumId;
            qDebug() << "ToolExecutor: Spotify album found:" << first["title"].toString() << "- fetching tracks";
            m_spotifyClient->getAlbum(albumId);
        } else if (m_pendingMusicType == "artists") {
            QVariantMap first = results.first().toMap();
            QString artistId = first["id"].toString();
            m_expectedSpotifyArtistId = artistId;
            qDebug() << "ToolExecutor: Spotify artist found:" << first["name"].toString() << "- fetching top tracks";
            m_spotifyClient->getArtist(artistId);
        } else {
            QVariantMap first = results.first().toMap();
            QString trackId = first["id"].toString();
            m_spotifyClient->playTrackInContext(trackId, results, 0);
            QJsonObject result;
            result["status"] = "playing";
            result["track"] = first["title"].toString();
            result["artist"] = first["artist"].toString();
            emit toolCompleted(m_pendingMusicToolId, result);
            m_pendingMusicToolId.clear();
        }
    });

    // When album data arrives, queue all tracks and play
    connect(client, &SpotifyClient::albumReceived, this, [this](const QVariantMap &album, const QVariantList &tracks) {
        if (m_pendingMusicToolId.isEmpty() || m_pendingMusicSource != "spotify") return;
        if (album["id"].toString() != m_expectedSpotifyAlbumId) return;

        QJsonObject result;
        if (tracks.isEmpty()) {
            result["status"] = "error";
            result["error"] = "Album has no playable tracks.";
        } else {
            QString firstId = tracks.first().toMap()["id"].toString();
            m_spotifyClient->playTrackInContext(firstId, tracks, 0);
            result["status"] = "playing";
            result["album"] = album["title"].toString();
            result["artist"] = album["artist"].toString();
            result["track_count"] = tracks.size();
        }
        emit toolCompleted(m_pendingMusicToolId, result);
        m_pendingMusicToolId.clear();
    });

    // When artist data arrives, queue top tracks and play
    connect(client, &SpotifyClient::artistReceived, this, [this](const QVariantMap &artist, const QVariantList &topTracks, const QVariantList &) {
        if (m_pendingMusicToolId.isEmpty() || m_pendingMusicSource != "spotify") return;
        if (artist["id"].toString() != m_expectedSpotifyArtistId) return;

        QJsonObject result;
        if (topTracks.isEmpty()) {
            result["status"] = "error";
            result["error"] = "No tracks found for this artist.";
        } else {
            QString firstId = topTracks.first().toMap()["id"].toString();
            m_spotifyClient->playTrackInContext(firstId, topTracks, 0);
            result["status"] = "playing";
            result["artist"] = artist["name"].toString();
            result["track"] = topTracks.first().toMap()["title"].toString();
            result["track_count"] = topTracks.size();
        }
        emit toolCompleted(m_pendingMusicToolId, result);
        m_pendingMusicToolId.clear();
    });

    // Handle Spotify errors during pending music tool (prevents chain hang)
    connect(client, &SpotifyClient::error, this, [this](const QString &message) {
        if (m_pendingMusicToolId.isEmpty() || m_pendingMusicSource != "spotify") return;
        QJsonObject result;
        result["status"] = "error";
        result["error"] = "Spotify error: " + message;
        emit toolCompleted(m_pendingMusicToolId, result);
        m_pendingMusicToolId.clear();
    });
}

// ========================================================================
// TOOL DISPATCH
// ========================================================================

QJsonObject ToolExecutor::executeTool(const QString &toolUseId, const QString &toolName, const QJsonObject &input)
{
    qDebug() << "ToolExecutor: Executing tool:" << toolName << "id:" << toolUseId;

    if (toolName == "navigate")          return handleNavigate(toolUseId, input);
    if (toolName == "add_stop")          return handleAddStop(toolUseId, input);
    if (toolName == "search_places")     return handleSearchPlaces(toolUseId, input);
    if (toolName == "call_contact")      return handleCallContact(toolUseId, input);
    if (toolName == "send_message")      return handleSendMessage(toolUseId, input);
    if (toolName == "read_messages")     return handleReadMessages(toolUseId, input);
    if (toolName == "play_music")        return handlePlayMusic(toolUseId, input);
    if (toolName == "control_playback")  return handleControlPlayback(toolUseId, input);
    if (toolName == "music_info")        return handleMusicInfo(toolUseId, input);
    if (toolName == "add_favorite")      return handleAddFavorite(toolUseId, input);
    if (toolName == "hangup_call")       return handleHangupCall(toolUseId, input);
    if (toolName == "answer_call")       return handleAnswerCall(toolUseId, input);
    if (toolName == "quiet_mode")        return handleQuietMode(toolUseId, input);
    if (toolName == "set_follow_up")     return handleSetFollowUp(toolUseId, input);
    if (toolName == "cancel_route")      return handleCancelRoute(toolUseId, input);

    qWarning() << "ToolExecutor: Unknown tool:" << toolName;
    QJsonObject result;
    result["status"] = "error";
    result["error"] = "Unknown tool: " + toolName;
    return result;
}

// ========================================================================
// TOOL HANDLERS
// ========================================================================

QJsonObject ToolExecutor::handleNavigate(const QString &/*toolUseId*/, const QJsonObject &input)
{
    QString destination = input["destination"].toString();
    if (destination.isEmpty()) {
        QJsonObject r; r["status"] = "error"; r["error"] = "No destination provided";
        return r;
    }

    emit navigationStarted(destination);

    QJsonObject result;
    result["status"] = "navigation_started";
    result["destination"] = destination;
    return result;
}

QJsonObject ToolExecutor::handleAddStop(const QString &/*toolUseId*/, const QJsonObject &input)
{
    QString destination = input["destination"].toString();
    if (destination.isEmpty()) {
        QJsonObject r; r["status"] = "error"; r["error"] = "No destination provided";
        return r;
    }

    emit routeStopRequested(destination);

    QJsonObject result;
    result["status"] = "stop_added";
    result["destination"] = destination;
    return result;
}

QJsonObject ToolExecutor::handleSearchPlaces(const QString &toolUseId, const QJsonObject &input)
{
    QString query = input["query"].toString();
    QString category = input["category"].toString();
    bool alongRoute = input["along_route"].toBool(false);
    QString near = input["near"].toString();

    if (query.isEmpty()) {
        QJsonObject r; r["status"] = "error"; r["error"] = "No search query provided";
        return r;
    }

    if (!m_placesSearchManager) {
        QJsonObject r; r["status"] = "error"; r["error"] = "Places search not available";
        return r;
    }

    // Async — store tool ID, result comes via searchCompleted signal
    m_pendingSearchToolId = toolUseId;
    m_placesSearchManager->searchPlaces(query, category, alongRoute, near);

    return QJsonObject(); // Empty = async, result via toolCompleted signal
}

QJsonObject ToolExecutor::handleCallContact(const QString &/*toolUseId*/, const QJsonObject &input)
{
    QString contactName = input["contact_name"].toString();
    QString phoneNumber = input["phone_number"].toString();

    if (phoneNumber.isEmpty() && !contactName.isEmpty()) {
        phoneNumber = findContactPhoneNumber(contactName);
    }

    if (phoneNumber.isEmpty()) {
        QJsonObject r; r["status"] = "error"; r["error"] = "Contact not found or no phone number";
        return r;
    }

    if (!m_bluetoothManager) {
        QJsonObject r; r["status"] = "error"; r["error"] = "Bluetooth not available";
        return r;
    }

    QMetaObject::invokeMethod(m_bluetoothManager, "dialNumber", Q_ARG(QString, phoneNumber));

    QJsonObject result;
    result["status"] = "calling";
    result["contact"] = contactName.isEmpty() ? phoneNumber : contactName;
    return result;
}

QJsonObject ToolExecutor::handleSendMessage(const QString &toolUseId, const QJsonObject &input)
{
    QString contactName = input["contact_name"].toString();
    QString messageBody = input["message_body"].toString();
    QString phoneNumber = input["phone_number"].toString();

    if (phoneNumber.isEmpty() && !contactName.isEmpty()) {
        phoneNumber = findContactPhoneNumber(contactName);
    }

    if (phoneNumber.isEmpty()) {
        QJsonObject r; r["status"] = "error"; r["error"] = "Contact not found or no phone number";
        return r;
    }
    if (messageBody.isEmpty()) {
        QJsonObject r; r["status"] = "error"; r["error"] = "No message body provided";
        return r;
    }

    // Store for confirmation flow
    m_pendingCommand.clear();
    m_pendingCommand["phone_number"] = phoneNumber;
    m_pendingCommand["message_body"] = messageBody;
    m_pendingCommand["contact_name"] = contactName;
    m_pendingToolUseId = toolUseId;
    m_pendingAction = "send_message";
    m_awaitingConfirmation = true;
    emit pendingActionChanged();
    emit awaitingConfirmationChanged();

    QString recipient = contactName.isEmpty() ? phoneNumber : contactName;
    emit confirmationRequested("send_message",
        QString("Send to %1: \"%2\"?").arg(recipient, messageBody));

    QJsonObject result;
    result["status"] = "awaiting_confirmation";
    result["recipient"] = recipient;
    result["message"] = messageBody;
    return result;
}

QJsonObject ToolExecutor::handleReadMessages(const QString &/*toolUseId*/, const QJsonObject &input)
{
    QString contactName = input["contact_name"].toString().toLower().trimmed();

    if (!m_messageManager) {
        QJsonObject r; r["status"] = "error"; r["error"] = "Messages not available";
        return r;
    }

    ConversationModel *convModel = m_messageManager->conversationModel();
    MessageModel *msgModel = m_messageManager->messageModel();

    if (!convModel || !msgModel) {
        QJsonObject r; r["status"] = "error"; r["error"] = "Message data not available";
        return r;
    }

    QJsonObject result;

    if (contactName.isEmpty()) {
        // No contact specified — return recent conversations summary
        const auto &conversations = convModel->conversations();
        if (conversations.isEmpty()) {
            result["status"] = "no_messages";
            result["message"] = "No message conversations found.";
            return result;
        }

        QJsonArray convArray;
        int limit = qMin(10, conversations.size());
        for (int i = 0; i < limit; ++i) {
            const auto &conv = conversations[i];
            QJsonObject c;
            c["contact"] = conv.contactName.isEmpty() ? conv.contactAddress : conv.contactName;
            c["last_message"] = conv.lastMessageBody;
            c["time"] = conv.lastMessageTime.toString("MMM d, h:mm AP");
            c["unread"] = conv.unreadCount;
            convArray.append(c);
        }

        result["status"] = "success";
        result["conversations"] = convArray;
        result["total"] = conversations.size();
    } else {
        // Contact specified — find matching conversation and return messages
        const auto &conversations = convModel->conversations();
        QString matchedThreadId;
        QString matchedName;

        for (const auto &conv : conversations) {
            if (conv.contactName.toLower().contains(contactName) ||
                conv.contactAddress.contains(contactName)) {
                matchedThreadId = conv.threadId;
                matchedName = conv.contactName.isEmpty() ? conv.contactAddress : conv.contactName;
                break;
            }
        }

        if (matchedThreadId.isEmpty()) {
            result["status"] = "no_messages";
            result["message"] = QString("No messages found for '%1'.").arg(input["contact_name"].toString());
            return result;
        }

        // Get messages for this thread
        const auto &allMessages = msgModel->messages();
        QJsonArray msgArray;
        int count = 0;
        // Walk backwards to get most recent first
        for (int i = allMessages.size() - 1; i >= 0 && count < 10; --i) {
            const auto &msg = allMessages[i];
            if (msg.threadId == matchedThreadId) {
                QJsonObject m;
                m["from"] = msg.isIncoming ? matchedName : "You";
                m["body"] = msg.body;
                m["time"] = msg.timestamp.toString("MMM d, h:mm AP");
                msgArray.append(m);
                count++;
            }
        }

        if (msgArray.isEmpty()) {
            // Thread exists but no individual messages loaded — return last message from conversation
            for (const auto &conv : conversations) {
                if (conv.threadId == matchedThreadId) {
                    QJsonObject m;
                    m["from"] = matchedName;
                    m["body"] = conv.lastMessageBody;
                    m["time"] = conv.lastMessageTime.toString("MMM d, h:mm AP");
                    msgArray.append(m);
                    break;
                }
            }
        }

        result["status"] = "success";
        result["contact"] = matchedName;
        result["messages"] = msgArray;
    }

    return result;
}

QJsonObject ToolExecutor::handlePlayMusic(const QString &toolUseId, const QJsonObject &input)
{
    QString query = input["query"].toString();
    QString source = input["source"].toString();
    QString type = input["type"].toString("tracks");  // Default to tracks

    if (query.isEmpty()) {
        QJsonObject r; r["status"] = "error"; r["error"] = "No search query provided";
        return r;
    }

    // Auto-detect source: prefer Tidal if logged in, then Spotify
    if (source.isEmpty()) {
        if (m_tidalClient && m_tidalClient->isLoggedIn()) source = "tidal";
        else if (m_spotifyClient && m_spotifyClient->isLoggedIn()) source = "spotify";
        else source = "tidal";
    }

    m_pendingMusicToolId = toolUseId;
    m_pendingMusicType = type;
    m_pendingMusicSource = source;
    m_musicGeneration++;

    if (source == "tidal" && m_tidalClient) {
        m_tidalClient->search(query, type, 5);
        return QJsonObject(); // Async
    } else if (source == "spotify" && m_spotifyClient) {
        m_spotifyClient->search(query, type, 5);
        return QJsonObject(); // Async
    }

    m_pendingMusicToolId.clear();
    QJsonObject r; r["status"] = "error"; r["error"] = "Music service not available";
    return r;
}

QJsonObject ToolExecutor::handleControlPlayback(const QString &/*toolUseId*/, const QJsonObject &input)
{
    QString command = input["command"].toString();
    QString source = input["source"].toString();
    qint64 seekMs = -1;
    if (input.contains("seek_ms")) {
        QJsonValue v = input["seek_ms"];
        // Handle both numeric and string types (Claude may send either)
        if (v.isString()) seekMs = v.toString().toLongLong();
        else seekMs = (qint64)v.toDouble();
    }

    // Auto-detect active source
    if (source.isEmpty()) {
        if (m_tidalClient && m_tidalClient->isPlaying()) source = "tidal";
        else if (m_spotifyClient && m_spotifyClient->isPlaying()) source = "spotify";
        else if (m_mediaController && m_mediaController->isPlaying()) source = "bluetooth";
        else if (m_tidalClient && m_tidalClient->isLoggedIn()) source = "tidal";
        else if (m_spotifyClient && m_spotifyClient->isLoggedIn()) source = "spotify";
        else source = "bluetooth";
    }

    // Handle seek
    if (seekMs >= 0) {
        if (source == "tidal" && m_tidalClient) m_tidalClient->seekTo(seekMs);
        else if (source == "spotify" && m_spotifyClient) m_spotifyClient->seekTo(seekMs);
        else if (m_mediaController) m_mediaController->seekTo(seekMs);

        QJsonObject result;
        result["status"] = "success";
        result["action"] = "seek";
        result["position_ms"] = seekMs;
        result["source"] = source;
        return result;
    }

    // Dispatch command to the right source
    auto doCommand = [&](auto *client) {
        if (!client) return;
        if (command == "play") client->play();
        else if (command == "pause") client->pause();
        else if (command == "next") client->next();
        else if (command == "previous") client->previous();
    };

    if (source == "tidal") {
        doCommand(m_tidalClient);
        if (command == "shuffle" && m_tidalClient) m_tidalClient->toggleShuffle();
        if (command == "repeat" && m_tidalClient) m_tidalClient->cycleRepeatMode();
    } else if (source == "spotify") {
        doCommand(m_spotifyClient);
        if (command == "shuffle" && m_spotifyClient) m_spotifyClient->toggleShuffle();
        if (command == "repeat" && m_spotifyClient) m_spotifyClient->cycleRepeatMode();
    } else {
        doCommand(m_mediaController);
    }

    QJsonObject result;
    if (command.isEmpty()) {
        result["status"] = "error";
        result["error"] = "No command specified";
    } else {
        result["status"] = "success";
        result["command"] = command;
        result["source"] = source;
    }
    return result;
}

QJsonObject ToolExecutor::handleQuietMode(const QString &/*toolUseId*/, const QJsonObject &input)
{
    bool enabled = input["enabled"].toBool(true);

    if (m_copilotMonitor) {
        m_copilotMonitor->setQuietMode(enabled);
    }

    QJsonObject result;
    result["status"] = "success";
    result["quiet_mode"] = enabled;
    return result;
}

QJsonObject ToolExecutor::handleSetFollowUp(const QString &/*toolUseId*/, const QJsonObject &/*input*/)
{
    emit followUpExpected();

    QJsonObject result;
    result["status"] = "ok";
    return result;
}

QJsonObject ToolExecutor::handleMusicInfo(const QString &/*toolUseId*/, const QJsonObject &/*input*/)
{
    QJsonObject result;

    // Find the active or paused source — check for track title even if paused
    QString source;
    bool playing = false;
    if (m_tidalClient && !m_tidalClient->trackTitle().isEmpty()) {
        source = "tidal"; playing = m_tidalClient->isPlaying();
    } else if (m_spotifyClient && !m_spotifyClient->trackTitle().isEmpty()) {
        source = "spotify"; playing = m_spotifyClient->isPlaying();
    } else if (m_mediaController && !m_mediaController->trackTitle().isEmpty()) {
        source = "bluetooth"; playing = m_mediaController->isPlaying();
    }

    if (source.isEmpty()) {
        result["status"] = "nothing_playing";
        result["message"] = "Nothing is currently playing.";
        return result;
    }

    result["status"] = "success";
    result["source"] = source;
    result["is_playing"] = playing;
    result["state"] = playing ? "playing" : "paused";

    if (source == "tidal" && m_tidalClient) {
        result["track"] = m_tidalClient->trackTitle();
        result["artist"] = m_tidalClient->artist();
        result["album"] = m_tidalClient->album();
        result["duration_ms"] = m_tidalClient->duration();
        result["position_ms"] = m_tidalClient->position();
        result["quality"] = m_tidalClient->audioQuality();
        result["shuffle"] = m_tidalClient->shuffleEnabled();
        result["repeat"] = m_tidalClient->repeatMode();
    } else if (source == "spotify" && m_spotifyClient) {
        result["track"] = m_spotifyClient->trackTitle();
        result["artist"] = m_spotifyClient->artist();
        result["album"] = m_spotifyClient->album();
        result["duration_ms"] = m_spotifyClient->duration();
        result["position_ms"] = m_spotifyClient->position();
        result["shuffle"] = m_spotifyClient->shuffleEnabled();
        result["repeat"] = m_spotifyClient->repeatMode();
    } else if (source == "bluetooth" && m_mediaController) {
        result["track"] = m_mediaController->trackTitle();
        result["artist"] = m_mediaController->artist();
        result["album"] = m_mediaController->album();
    }

    return result;
}

QJsonObject ToolExecutor::handleAddFavorite(const QString &/*toolUseId*/, const QJsonObject &input)
{
    bool remove = input["remove"].toBool(false);

    // Find the active source and current track
    if (m_tidalClient && m_tidalClient->isPlaying()) {
        // Get current track ID from queue
        QVariantList queue = m_tidalClient->queue();
        int pos = m_tidalClient->queuePosition();
        if (pos >= 0 && pos < queue.size()) {
            int trackId = queue[pos].toMap()["id"].toInt();
            if (remove) m_tidalClient->removeFavorite(trackId);
            else m_tidalClient->addFavorite(trackId);

            QJsonObject result;
            result["status"] = "success";
            result["action"] = remove ? "removed" : "added";
            result["track"] = m_tidalClient->trackTitle();
            result["source"] = "tidal";
            return result;
        }
    } else if (m_spotifyClient && m_spotifyClient->isPlaying()) {
        QVariantList queue = m_spotifyClient->queue();
        int pos = m_spotifyClient->queuePosition();
        if (pos >= 0 && pos < queue.size()) {
            QString trackId = queue[pos].toMap()["id"].toString();
            if (remove) m_spotifyClient->removeFavorite(trackId);
            else m_spotifyClient->addFavorite(trackId);

            QJsonObject result;
            result["status"] = "success";
            result["action"] = remove ? "removed" : "added";
            result["track"] = m_spotifyClient->trackTitle();
            result["source"] = "spotify";
            return result;
        }
    }

    QJsonObject r; r["status"] = "error"; r["error"] = "No track is currently playing";
    return r;
}

QJsonObject ToolExecutor::handleHangupCall(const QString &/*toolUseId*/, const QJsonObject &/*input*/)
{
    if (!m_bluetoothManager) {
        QJsonObject r; r["status"] = "error"; r["error"] = "Bluetooth not available";
        return r;
    }

    QMetaObject::invokeMethod(m_bluetoothManager, "hangupCall");

    QJsonObject result;
    result["status"] = "success";
    result["action"] = "call_ended";
    return result;
}

QJsonObject ToolExecutor::handleAnswerCall(const QString &/*toolUseId*/, const QJsonObject &/*input*/)
{
    if (!m_bluetoothManager) {
        QJsonObject r; r["status"] = "error"; r["error"] = "Bluetooth not available";
        return r;
    }

    QMetaObject::invokeMethod(m_bluetoothManager, "answerCall");

    QJsonObject result;
    result["status"] = "success";
    result["action"] = "call_answered";
    return result;
}

// ========================================================================
// CONFIRMATION FLOW
// ========================================================================

void ToolExecutor::confirmAction()
{
    if (!m_awaitingConfirmation) return;

    if (m_pendingAction == "send_message" && m_messageManager) {
        QString phoneNumber = m_pendingCommand["phone_number"].toString();
        QString messageBody = m_pendingCommand["message_body"].toString();

        QMetaObject::invokeMethod(m_messageManager, "sendMessage",
                                  Q_ARG(QString, phoneNumber),
                                  Q_ARG(QString, messageBody));

        qDebug() << "ToolExecutor: Message sent to" << phoneNumber;
    }

    m_awaitingConfirmation = false;
    m_pendingAction.clear();
    m_pendingCommand.clear();
    m_pendingToolUseId.clear();
    emit awaitingConfirmationChanged();
    emit pendingActionChanged();
}

void ToolExecutor::cancelAction()
{
    m_awaitingConfirmation = false;
    m_pendingAction.clear();
    m_pendingCommand.clear();
    m_pendingToolUseId.clear();
    emit awaitingConfirmationChanged();
    emit pendingActionChanged();
}

// ========================================================================
// HELPERS
// ========================================================================

void ToolExecutor::clearPendingTools()
{
    if (!m_pendingSearchToolId.isEmpty()) {
        qDebug() << "ToolExecutor: Clearing pending search tool:" << m_pendingSearchToolId;
        m_pendingSearchToolId.clear();
    }
    if (!m_pendingMusicToolId.isEmpty()) {
        qDebug() << "ToolExecutor: Clearing pending music tool:" << m_pendingMusicToolId;
        m_pendingMusicToolId.clear();
        m_pendingMusicType.clear();
        m_pendingMusicSource.clear();
    }
    m_musicGeneration++;
}

QJsonObject ToolExecutor::handleCancelRoute(const QString &/*toolUseId*/, const QJsonObject &/*input*/)
{
    emit routeCancelled();

    QJsonObject result;
    result["status"] = "route_cancelled";
    return result;
}

QString ToolExecutor::findContactPhoneNumber(const QString &contactName)
{
    if (!m_contactManager) return QString();

    QVariantList results;
    QMetaObject::invokeMethod(m_contactManager, "searchContacts",
                              Q_RETURN_ARG(QVariantList, results),
                              Q_ARG(QString, contactName));

    if (results.isEmpty()) {
        qDebug() << "ToolExecutor: No contact found for:" << contactName;
        return QString();
    }

    QVariantMap contact = results.first().toMap();
    QString phoneNumber = contact["phoneNumber"].toString();
    qDebug() << "ToolExecutor: Found" << contact["name"].toString() << "->" << phoneNumber;
    return phoneNumber;
}
