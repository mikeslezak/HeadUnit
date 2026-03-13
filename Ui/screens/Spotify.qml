import QtQuick 2.15
import QtQuick.Controls 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

Item {
    id: root
    anchors.fill: parent
    clip: true
    property var theme: null

    // View stack: "home", "search", "favorites", "album", "artist", "playlist", "nowplaying"
    property string currentView: "home"
    property string searchQuery: ""
    property string searchType: "tracks"
    property var browseData: null
    property var browseTracks: []
    property bool queueVisible: false

    // Helpers
    function formatMs(ms) {
        var totalSec = Math.floor(ms / 1000)
        var mins = Math.floor(totalSec / 60)
        var secs = totalSec % 60
        return mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    function formatDuration(ms) {
        var totalSec = Math.floor(ms / 1000)
        var mins = Math.floor(totalSec / 60)
        var secs = totalSec % 60
        return mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    function iconSource(key) {
        return (theme && theme.iconPath) ? theme.iconPath(key) : ""
    }

    // Store artist albums separately
    property var artistAlbums: []
    property var homeFavorites: []

    Connections {
        target: spotifyClient

        function onAuthStatusChanged() {
            if (spotifyClient.isLoggedIn && currentView === "home") {
                spotifyClient.getFavorites()
            }
        }

        function onSearchResultsChanged() {
            currentView = "search"
        }

        function onAlbumReceived(album, tracks) {
            browseData = album
            browseTracks = tracks
            currentView = "album"
        }

        function onArtistReceived(artist, topTracks, albums) {
            browseData = artist
            artistAlbums = albums
            browseTracks = topTracks
            currentView = "artist"
        }

        function onFavoritesReceived(tracks) {
            homeFavorites = tracks
            if (currentView === "home" || currentView === "favorites") {
                browseTracks = tracks
                currentView = "favorites"
            }
        }

        function onPlaylistReceived(playlist, tracks) {
            browseData = playlist
            browseTracks = tracks
            currentView = "playlist"
        }

        function onPlaylistsReceived(playlists) {
            browseTracks = playlists
            currentView = "playlists"
        }

        function onPlayStateChanged() {
            if (spotifyClient.isPlaying && currentView !== "nowplaying") {
                currentView = "nowplaying"
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        // ================================================================
        // BROWSE MODE (search, favorites, album, artist, playlist, home)
        // ================================================================
        Item {
            id: browseView
            anchors.fill: parent
            anchors.margins: 16
            visible: opacity > 0
            opacity: currentView !== "nowplaying" ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

            Column {
                anchors.fill: parent
                spacing: 10

                // ── Top Bar: Back + Search + Now Playing indicator + Status ──
                Row {
                    width: parent.width
                    height: 48
                    spacing: 10

                    // Back button
                    Rectangle {
                        width: 48; height: 48
                        radius: ThemeValues.radius
                        color: backMa.pressed
                            ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                            : "transparent"
                        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                        border.width: 1
                        visible: currentView !== "home" && currentView !== "favorites"
                                 && currentView !== "search" && currentView !== "playlists"

                        Text {
                            anchors.centerIn: parent
                            text: "←"
                            color: ThemeValues.textCol
                            font.pixelSize: 22
                            font.family: ThemeValues.fontFamily
                        }

                        MouseArea {
                            id: backMa
                            anchors.fill: parent
                            onClicked: {
                                if (currentView === "album" || currentView === "artist" || currentView === "playlist")
                                    currentView = "search"
                                else currentView = "home"
                            }
                        }
                    }

                    // Search field
                    Rectangle {
                        width: parent.width
                              - (currentView !== "home" && currentView !== "favorites"
                                 && currentView !== "search" && currentView !== "playlists" ? 58 : 0)
                              - (spotifyClient.isPlaying ? 58 : 0)
                              - statusCol.width - 30
                        height: 48
                        radius: ThemeValues.radius
                        color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                        border.color: searchInput.activeFocus
                            ? ThemeValues.primaryCol
                            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            Canvas {
                                width: 16; height: 16
                                anchors.verticalCenter: parent.verticalCenter
                                opacity: 0.5
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = ThemeValues.textCol.toString(); ctx.lineWidth = 1.5; ctx.lineCap = "round"
                                    ctx.beginPath(); ctx.arc(7, 7, 5, 0, Math.PI * 2); ctx.stroke()
                                    ctx.beginPath(); ctx.moveTo(11, 11); ctx.lineTo(15, 15); ctx.stroke()
                                }
                            }

                            TextInput {
                                id: searchInput
                                width: parent.width - 28
                                anchors.verticalCenter: parent.verticalCenter
                                color: ThemeValues.textCol
                                font.pixelSize: ThemeValues.fontSize
                                font.family: ThemeValues.fontFamily
                                clip: true

                                onAccepted: {
                                    if (text.trim().length > 0) {
                                        searchQuery = text.trim()
                                        spotifyClient.search(searchQuery, searchType)
                                    }
                                }

                                Text {
                                    visible: !parent.text && !parent.activeFocus
                                    text: "Search Spotify..."
                                    color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                                    font: parent.font
                                }
                            }
                        }
                    }

                    // Now Playing button (visible when something is playing)
                    Rectangle {
                        width: 48; height: 48
                        radius: ThemeValues.radius
                        visible: spotifyClient.isPlaying
                        color: npBtnMa.pressed
                            ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                        border.color: ThemeValues.primaryCol
                        border.width: 1

                        // Animated equalizer bars
                        Row {
                            anchors.centerIn: parent
                            spacing: 3
                            Repeater {
                                model: 3
                                Rectangle {
                                    width: 4
                                    height: 8 + index * 4
                                    color: ThemeValues.primaryCol
                                    anchors.bottom: parent.bottom
                                    SequentialAnimation on height {
                                        running: spotifyClient.isPlaying
                                        loops: Animation.Infinite
                                        NumberAnimation { to: 6 + Math.random() * 12; duration: 300 + index * 100 }
                                        NumberAnimation { to: 12 + Math.random() * 8; duration: 300 + index * 100 }
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: npBtnMa
                            anchors.fill: parent
                            onClicked: currentView = "nowplaying"
                        }
                    }

                    // Status indicator
                    Column {
                        id: statusCol
                        width: 120
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Row {
                            spacing: 6

                            Rectangle {
                                width: 8; height: 8; radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: spotifyClient.isLoggedIn ? ThemeValues.successCol
                                     : spotifyClient.isConnected ? ThemeValues.warningCol
                                     : ThemeValues.errorCol
                            }

                            Text {
                                text: spotifyClient.isLoggedIn ? spotifyClient.userName
                                    : spotifyClient.isConnected ? "Not logged in"
                                    : "Offline"
                                color: ThemeValues.textCol
                                font.pixelSize: ThemeValues.fontSize - 4
                                font.family: ThemeValues.fontFamily
                                elide: Text.ElideRight
                                width: 106
                            }
                        }

                        Text {
                            visible: spotifyClient.isLoading
                            text: spotifyClient.statusMessage
                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                            font.pixelSize: ThemeValues.fontSize - 6
                            font.family: ThemeValues.fontFamily
                            elide: Text.ElideRight
                            width: 120
                        }
                    }
                }

                // ── Search Type Tabs ──
                Row {
                    width: parent.width
                    height: 44
                    spacing: 8
                    visible: currentView === "search" || currentView === "home"
                             || currentView === "favorites" || currentView === "playlists"

                    Repeater {
                        model: [
                            { key: "tracks", label: "Tracks" },
                            { key: "albums", label: "Albums" },
                            { key: "artists", label: "Artists" },
                            { key: "favorites", label: "Liked" },
                            { key: "playlists", label: "Playlists" }
                        ]

                        Rectangle {
                            width: (parent.width - 32) / 5
                            height: 44
                            radius: ThemeValues.radius
                            color: (searchType === modelData.key
                                    || (modelData.key === "favorites" && currentView === "favorites")
                                    || (modelData.key === "playlists" && currentView === "playlists"))
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                                : "transparent"
                            border.color: (searchType === modelData.key
                                    || (modelData.key === "favorites" && currentView === "favorites")
                                    || (modelData.key === "playlists" && currentView === "playlists"))
                                ? ThemeValues.primaryCol
                                : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: (searchType === modelData.key
                                        || (modelData.key === "favorites" && currentView === "favorites")
                                        || (modelData.key === "playlists" && currentView === "playlists"))
                                    ? ThemeValues.primaryCol : ThemeValues.textCol
                                font.pixelSize: ThemeValues.fontSize - 2
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (modelData.key === "favorites") {
                                        currentView = "favorites"
                                        spotifyClient.getFavorites()
                                    } else if (modelData.key === "playlists") {
                                        currentView = "playlists"
                                        spotifyClient.getPlaylists()
                                    } else {
                                        searchType = modelData.key
                                        currentView = "search"
                                        if (searchQuery.length > 0) {
                                            spotifyClient.search(searchQuery, searchType)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ── Login Screen ──
                Item {
                    width: parent.width
                    height: parent.height - 110
                    visible: !spotifyClient.isLoggedIn && spotifyClient.isConnected

                    Column {
                        anchors.centerIn: parent
                        spacing: 20
                        width: 400

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "SPOTIFY"
                            color: ThemeValues.primaryCol
                            font.pixelSize: 48
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Sign in to access your music"
                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.6)
                            font.pixelSize: ThemeValues.fontSize
                            font.family: ThemeValues.fontFamily
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 300; height: 64
                            radius: ThemeValues.radius
                            color: loginMa.pressed
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                                : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                            border.color: ThemeValues.primaryCol
                            border.width: 2
                            visible: spotifyClient.loginUrl === ""

                            Text {
                                anchors.centerIn: parent
                                text: "Sign In with Spotify"
                                color: ThemeValues.primaryCol
                                font.pixelSize: ThemeValues.fontSize
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                            }

                            MouseArea {
                                id: loginMa
                                anchors.fill: parent
                                onClicked: spotifyClient.startLogin()
                            }
                        }

                        Column {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 12
                            visible: spotifyClient.loginUrl !== ""

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Open this URL on your phone:"
                                color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.6)
                                font.pixelSize: ThemeValues.fontSize - 2
                                font.family: ThemeValues.fontFamily
                            }

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 380; height: 56
                                radius: ThemeValues.radius
                                color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.8)
                                border.color: ThemeValues.primaryCol
                                border.width: 2

                                Text {
                                    anchors.centerIn: parent
                                    width: parent.width - 20
                                    text: spotifyClient.loginUrl
                                    color: ThemeValues.primaryCol
                                    font.pixelSize: 13
                                    font.family: ThemeValues.fontFamily
                                    font.weight: Font.Bold
                                    elide: Text.ElideMiddle
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Waiting for authorization..."
                                color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.4)
                                font.pixelSize: ThemeValues.fontSize - 3
                                font.family: ThemeValues.fontFamily

                                SequentialAnimation on opacity {
                                    running: spotifyClient.loginUrl !== ""
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 800 }
                                    NumberAnimation { to: 1.0; duration: 800 }
                                }
                            }
                        }
                    }
                }

                // ── Offline Screen ──
                Item {
                    width: parent.width
                    height: parent.height - 110
                    visible: !spotifyClient.isConnected

                    Column {
                        anchors.centerIn: parent
                        spacing: 16

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "SPOTIFY"
                            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                            font.pixelSize: 48
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Connecting to Spotify service..."
                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                            font.pixelSize: ThemeValues.fontSize
                            font.family: ThemeValues.fontFamily
                        }
                    }
                }

                // ── Content area (tracks, albums, artists, playlists, loading) ──
                Item {
                    width: parent.width
                    height: parent.height - 110
                    visible: spotifyClient.isLoggedIn && (currentView === "search" || currentView === "favorites"
                             || currentView === "album" || currentView === "playlist")

                    // ── Track List (search tracks, favorites, album tracks, playlist tracks) ──
                    ListView {
                        id: trackList
                        anchors.fill: parent
                        clip: true
                        spacing: 4
                        visible: searchType === "tracks" || currentView === "favorites"
                                 || currentView === "album" || currentView === "playlist"
                        model: currentView === "search" ? spotifyClient.searchResults
                             : currentView === "album" ? browseTracks
                             : currentView === "playlist" ? browseTracks
                             : currentView === "favorites" ? browseTracks
                             : []

                        header: (currentView === "album" || currentView === "playlist") && browseData
                                ? albumHeaderComponent : null

                        delegate: Rectangle {
                            width: trackList.width
                            height: 76
                            radius: ThemeValues.radius
                            property bool isCurrentTrack: spotifyClient.isPlaying &&
                                modelData.id && modelData.title === spotifyClient.trackTitle
                            color: trackMa.pressed
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.25)
                                : isCurrentTrack
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.12)
                                : Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.25)
                            border.color: isCurrentTrack ? ThemeValues.primaryCol
                                : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.1)
                            border.width: 1

                            Behavior on color { ColorAnimation { duration: 100 } }

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                anchors.topMargin: 6
                                anchors.bottomMargin: 6
                                spacing: 12

                                // Album art with now-playing equalizer overlay
                                Rectangle {
                                    width: 64; height: 64
                                    radius: 6
                                    color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.8)
                                    anchors.verticalCenter: parent.verticalCenter

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 1
                                        source: modelData.image_url || ""
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                        smooth: true
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "♪"
                                        color: ThemeValues.primaryCol
                                        font.pixelSize: 24
                                        opacity: 0.3
                                        visible: parent.children[0].status !== Image.Ready && !isCurrentTrack
                                    }

                                    // Now-playing equalizer bars overlay
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 6
                                        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.55)
                                        visible: isCurrentTrack

                                        Row {
                                            anchors.centerIn: parent
                                            spacing: 3
                                            Repeater {
                                                model: 4
                                                Rectangle {
                                                    width: 4
                                                    property real baseH: 8 + index * 3
                                                    height: baseH
                                                    color: ThemeValues.primaryCol
                                                    anchors.bottom: parent.bottom
                                                    radius: 1

                                                    SequentialAnimation on height {
                                                        running: isCurrentTrack && spotifyClient.isPlaying
                                                        loops: Animation.Infinite
                                                        NumberAnimation {
                                                            to: 6 + Math.random() * 16
                                                            duration: 280 + index * 80
                                                            easing.type: Easing.InOutQuad
                                                        }
                                                        NumberAnimation {
                                                            to: 10 + Math.random() * 10
                                                            duration: 280 + index * 80
                                                            easing.type: Easing.InOutQuad
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                Column {
                                    width: parent.width - 64 - durationCol.width - 36
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4

                                    // Title row with explicit badge
                                    Row {
                                        width: parent.width
                                        spacing: 6

                                        // Explicit badge
                                        Rectangle {
                                            width: 18; height: 18
                                            radius: 3
                                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.2)
                                            visible: modelData.explicit === true
                                            anchors.verticalCenter: parent.verticalCenter

                                            Text {
                                                anchors.centerIn: parent
                                                text: "E"
                                                color: ThemeValues.textCol
                                                font.pixelSize: 11
                                                font.family: ThemeValues.fontFamily
                                                font.weight: Font.Bold
                                            }
                                        }

                                        Text {
                                            width: parent.width - (modelData.explicit ? 24 : 0)
                                            text: modelData.title || "Unknown"
                                            color: isCurrentTrack ? ThemeValues.primaryCol : ThemeValues.textCol
                                            font.pixelSize: 18
                                            font.family: ThemeValues.fontFamily
                                            font.weight: Font.Bold
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Row {
                                        width: parent.width
                                        spacing: 6

                                        Text {
                                            text: modelData.artist || ""
                                            color: isCurrentTrack ? ThemeValues.primaryCol
                                                : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.7)
                                            font.pixelSize: 15
                                            font.family: ThemeValues.fontFamily
                                            elide: Text.ElideRight
                                            width: Math.min(implicitWidth, parent.width * 0.5)
                                        }

                                        Text {
                                            text: modelData.album ? "· " + modelData.album : ""
                                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.45)
                                            font.pixelSize: 15
                                            font.family: ThemeValues.fontFamily
                                            elide: Text.ElideRight
                                            width: Math.min(implicitWidth, parent.width * 0.4)
                                        }
                                    }
                                }

                                // Duration
                                Column {
                                    id: durationCol
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2

                                    Text {
                                        text: formatDuration(modelData.duration_ms || 0)
                                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.45)
                                        font.pixelSize: 15
                                        font.family: ThemeValues.fontFamily
                                        anchors.right: parent.right
                                    }
                                }
                            }

                            MouseArea {
                                id: trackMa
                                anchors.fill: parent
                                onClicked: {
                                    if (modelData.id) {
                                        var trackListModel = trackList.model
                                        spotifyClient.playTrackInContext(modelData.id, trackListModel, index)
                                    }
                                }
                            }
                        }
                    }

                    // ── Album Search Results ──
                    GridView {
                        id: albumGrid
                        anchors.fill: parent
                        clip: true
                        visible: currentView === "search" && searchType === "albums"
                        model: spotifyClient.searchResults
                        cellWidth: Math.floor(parent.width / 4)
                        cellHeight: 200

                        delegate: Item {
                            width: albumGrid.cellWidth
                            height: 200

                            Column {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                Rectangle {
                                    width: parent.width
                                    height: width
                                    radius: 8
                                    color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                                    border.color: albumCardMa.pressed ? ThemeValues.primaryCol
                                        : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                                    border.width: 1

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: modelData.image_url || ""
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                        smooth: true
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "♪"
                                        color: ThemeValues.primaryCol
                                        font.pixelSize: 32
                                        opacity: 0.2
                                        visible: parent.children[0].status !== Image.Ready
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.title || ""
                                    color: ThemeValues.textCol
                                    font.pixelSize: 14
                                    font.family: ThemeValues.fontFamily
                                    font.weight: Font.Bold
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.artist || ""
                                    color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                                    font.pixelSize: 13
                                    font.family: ThemeValues.fontFamily
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: albumCardMa
                                anchors.fill: parent
                                onClicked: {
                                    if (modelData.id) spotifyClient.getAlbum(modelData.id)
                                }
                            }
                        }
                    }

                    // ── Artist Search Results ──
                    GridView {
                        id: artistGrid
                        anchors.fill: parent
                        clip: true
                        visible: currentView === "search" && searchType === "artists"
                        model: spotifyClient.searchResults
                        cellWidth: Math.floor(parent.width / 4)
                        cellHeight: 200

                        delegate: Item {
                            width: artistGrid.cellWidth
                            height: 200

                            Column {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                Rectangle {
                                    width: Math.min(parent.width, 120)
                                    height: width
                                    radius: width / 2
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                                    border.color: artistCardMa.pressed ? ThemeValues.primaryCol
                                        : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                                    border.width: 1

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: modelData.image_url || ""
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                        smooth: true
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "♪"
                                        color: ThemeValues.primaryCol
                                        font.pixelSize: 32
                                        opacity: 0.2
                                        visible: parent.children[0].status !== Image.Ready
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.name || ""
                                    color: ThemeValues.textCol
                                    font.pixelSize: 15
                                    font.family: ThemeValues.fontFamily
                                    font.weight: Font.Bold
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: artistCardMa
                                anchors.fill: parent
                                onClicked: {
                                    if (modelData.id) spotifyClient.getArtist(modelData.id)
                                }
                            }
                        }
                    }

                    // Empty state
                    Text {
                        visible: {
                            if (spotifyClient.isLoading) return false
                            if (currentView === "search") {
                                return spotifyClient.searchResults.length === 0
                            }
                            if (currentView === "favorites" || currentView === "album"
                                || currentView === "playlist") {
                                return browseTracks.length === 0
                            }
                            return false
                        }
                        anchors.centerIn: parent
                        text: currentView === "search" ? "Search for music above"
                            : currentView === "favorites" ? "No liked songs yet"
                            : "No tracks"
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                        font.pixelSize: ThemeValues.fontSize
                        font.family: ThemeValues.fontFamily
                    }

                    // Loading indicator
                    Text {
                        visible: spotifyClient.isLoading
                        anchors.centerIn: parent
                        text: spotifyClient.statusMessage
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                        font.pixelSize: ThemeValues.fontSize
                        font.family: ThemeValues.fontFamily

                        SequentialAnimation on opacity {
                            running: spotifyClient.isLoading
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 600 }
                            NumberAnimation { to: 1.0; duration: 600 }
                        }
                    }
                }

                // ── Playlists Grid ──
                Item {
                    width: parent.width
                    height: parent.height - 110
                    visible: spotifyClient.isLoggedIn && currentView === "playlists"

                    GridView {
                        id: playlistGrid
                        anchors.fill: parent
                        clip: true
                        model: browseTracks
                        cellWidth: Math.floor(parent.width / 4)
                        cellHeight: 200

                        delegate: Item {
                            width: playlistGrid.cellWidth
                            height: 200

                            Column {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                Rectangle {
                                    width: parent.width
                                    height: width
                                    radius: 8
                                    color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                                    border.color: plCardMa.pressed ? ThemeValues.primaryCol
                                        : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                                    border.width: 1

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 2
                                        source: modelData.image_url || ""
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                        smooth: true
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: "♪"
                                        color: ThemeValues.primaryCol
                                        font.pixelSize: 32
                                        opacity: 0.2
                                        visible: parent.children[0].status !== Image.Ready
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: modelData.name || ""
                                    color: ThemeValues.textCol
                                    font.pixelSize: 14
                                    font.family: ThemeValues.fontFamily
                                    font.weight: Font.Bold
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: (modelData.num_tracks || 0) + " tracks"
                                    color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                                    font.pixelSize: 13
                                    font.family: ThemeValues.fontFamily
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: plCardMa
                                anchors.fill: parent
                                onClicked: {
                                    if (modelData.id) spotifyClient.getPlaylist(modelData.id)
                                }
                            }
                        }
                    }

                    Text {
                        visible: !spotifyClient.isLoading && browseTracks.length === 0
                        anchors.centerIn: parent
                        text: "No playlists"
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                        font.pixelSize: ThemeValues.fontSize
                        font.family: ThemeValues.fontFamily
                    }
                }

                // ── Artist View ──
                Item {
                    id: artistViewItem
                    width: parent.width
                    height: parent.height - 110
                    visible: spotifyClient.isLoggedIn && currentView === "artist" && browseData

                    // Left column: artist photo
                    Item {
                        id: artistLeftCol
                        width: 160
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        Column {
                            width: parent.width
                            spacing: 12

                            Rectangle {
                                width: 120; height: 120
                                radius: 60
                                color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                                border.color: ThemeValues.primaryCol
                                border.width: 1
                                anchors.horizontalCenter: parent.horizontalCenter

                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 2
                                    source: browseData ? (browseData.image_url || "") : ""
                                    fillMode: Image.PreserveAspectCrop
                                    visible: status === Image.Ready
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "♪"
                                    color: ThemeValues.primaryCol
                                    font.pixelSize: 40
                                    opacity: 0.3
                                }
                            }

                            Text {
                                width: parent.width
                                text: browseData ? (browseData.name || "") : ""
                                color: ThemeValues.textCol
                                font.pixelSize: ThemeValues.fontSize + 4
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                            }

                            // Play all button
                            Rectangle {
                                width: 140; height: 44
                                radius: ThemeValues.radius
                                anchors.horizontalCenter: parent.horizontalCenter
                                color: playAllArtMa.pressed
                                    ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                                    : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                                border.color: ThemeValues.primaryCol
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "▶  Play All"
                                    color: ThemeValues.primaryCol
                                    font.pixelSize: ThemeValues.fontSize - 2
                                    font.family: ThemeValues.fontFamily
                                    font.weight: Font.Bold
                                }

                                MouseArea {
                                    id: playAllArtMa
                                    anchors.fill: parent
                                    onClicked: {
                                        if (browseTracks.length > 0) {
                                            var firstTrack = browseTracks[0]
                                            spotifyClient.playTrackInContext(firstTrack.id, browseTracks, 0)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Right column: top tracks + albums
                    Item {
                        anchors.left: artistLeftCol.right
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        Text {
                            id: topTracksLabel
                            text: "TOP TRACKS"
                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                            font.pixelSize: ThemeValues.fontSize - 3
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                        }

                        ListView {
                            id: artistTrackList
                            anchors.top: topTracksLabel.bottom
                            anchors.topMargin: 8
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: artistAlbumsSection.top
                            anchors.bottomMargin: 8
                            clip: true
                            spacing: 4
                            model: browseTracks

                            delegate: Rectangle {
                                width: artistTrackList.width
                                height: 68
                                radius: ThemeValues.radius
                                color: artTrackMa.pressed
                                    ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                                    : Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.2)
                                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.1)
                                border.width: 1

                                Row {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 12

                                    Text {
                                        width: 28
                                        text: (index + 1) + "."
                                        color: ThemeValues.primaryCol
                                        font.pixelSize: ThemeValues.fontSize - 2
                                        font.family: ThemeValues.fontFamily
                                        anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignRight
                                    }

                                    Column {
                                        width: parent.width - 40 - artDurText.width - 12
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 3

                                        Text {
                                            width: parent.width
                                            text: modelData.title || ""
                                            color: ThemeValues.textCol
                                            font.pixelSize: 18
                                            font.family: ThemeValues.fontFamily
                                            font.weight: Font.Bold
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            text: modelData.album || ""
                                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.45)
                                            font.pixelSize: 15
                                            font.family: ThemeValues.fontFamily
                                            elide: Text.ElideRight
                                            width: parent.width
                                        }
                                    }

                                    Text {
                                        id: artDurText
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: formatDuration(modelData.duration_ms || 0)
                                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.45)
                                        font.pixelSize: 15
                                        font.family: ThemeValues.fontFamily
                                    }
                                }

                                MouseArea {
                                    id: artTrackMa
                                    anchors.fill: parent
                                    onClicked: {
                                        if (modelData.id) {
                                            spotifyClient.playTrackInContext(modelData.id, browseTracks, index)
                                        }
                                    }
                                }
                            }
                        }

                        // ── Artist Albums horizontal scroll ──
                        Item {
                            id: artistAlbumsSection
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: artistAlbums.length > 0 ? 160 : 0
                            visible: artistAlbums.length > 0

                            Text {
                                id: albumsLabel
                                text: "ALBUMS"
                                color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                                font.pixelSize: ThemeValues.fontSize - 3
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                            }

                            ListView {
                                anchors.top: albumsLabel.bottom
                                anchors.topMargin: 6
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                orientation: ListView.Horizontal
                                clip: true
                                spacing: 12
                                model: artistAlbums

                                delegate: Item {
                                    width: 110
                                    height: parent.height

                                    Column {
                                        anchors.fill: parent
                                        spacing: 4

                                        Rectangle {
                                            width: 100; height: 100
                                            radius: 6
                                            color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                                            border.color: artAlbumMa.pressed ? ThemeValues.primaryCol
                                                : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                                            border.width: 1

                                            Image {
                                                anchors.fill: parent
                                                anchors.margins: 2
                                                source: modelData.image_url || ""
                                                fillMode: Image.PreserveAspectCrop
                                                visible: status === Image.Ready
                                                smooth: true
                                            }
                                        }

                                        Text {
                                            width: 100
                                            text: modelData.title || ""
                                            color: ThemeValues.textCol
                                            font.pixelSize: 12
                                            font.family: ThemeValues.fontFamily
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            width: 100
                                            text: modelData.year ? String(modelData.year) : ""
                                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.4)
                                            font.pixelSize: 11
                                            font.family: ThemeValues.fontFamily
                                        }
                                    }

                                    MouseArea {
                                        id: artAlbumMa
                                        anchors.fill: parent
                                        onClicked: {
                                            if (modelData.id) spotifyClient.getAlbum(modelData.id)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ================================================================
        // NOW PLAYING VIEW
        // ================================================================
        Item {
            id: nowPlayingView
            anchors.fill: parent
            visible: opacity > 0
            opacity: currentView === "nowplaying" ? 1 : 0
            x: currentView === "nowplaying" ? 0 : 80
            Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
            Behavior on x { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }

            // Back to browse button
            Rectangle {
                id: npBackBtn
                x: 16; y: 16
                width: 48; height: 48
                radius: ThemeValues.radius
                z: 10
                color: npBackMa.pressed
                    ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                    : "transparent"
                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "←"
                    color: ThemeValues.textCol
                    font.pixelSize: 22
                    font.family: ThemeValues.fontFamily
                }

                MouseArea {
                    id: npBackMa
                    anchors.fill: parent
                    onClicked: currentView = "search"
                }
            }

            Row {
                anchors.fill: parent
                anchors.margins: 20
                anchors.topMargin: 16
                spacing: 40

                // ── Left Column: Album Art ──
                Item {
                    width: parent.width * 0.38
                    height: parent.height

                    Rectangle {
                        id: albumArtContainer
                        width: Math.min(parent.width - 20, 280)
                        height: width
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -20
                        radius: 12
                        color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                        border.width: 2

                        // Crossfade album art
                        property string currentArtUrl: spotifyClient.albumArtUrl || ""
                        property bool showArtA: true

                        onCurrentArtUrlChanged: {
                            if (showArtA) {
                                npAlbumArtB.source = currentArtUrl;
                            } else {
                                npAlbumArtA.source = currentArtUrl;
                            }
                            showArtA = !showArtA;
                        }

                        Image {
                            id: npAlbumArtA
                            anchors.fill: parent
                            anchors.margins: 2
                            source: albumArtContainer.currentArtUrl
                            fillMode: Image.PreserveAspectCrop
                            opacity: albumArtContainer.showArtA ? 1.0 : 0.0
                            visible: opacity > 0 || status === Image.Loading
                            smooth: true
                            Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.InOutQuad } }
                        }

                        Image {
                            id: npAlbumArtB
                            anchors.fill: parent
                            anchors.margins: 2
                            fillMode: Image.PreserveAspectCrop
                            opacity: albumArtContainer.showArtA ? 0.0 : 1.0
                            visible: opacity > 0 || status === Image.Loading
                            smooth: true
                            Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.InOutQuad } }
                        }

                        // Fallback icon
                        Text {
                            anchors.centerIn: parent
                            text: "♪"
                            color: ThemeValues.primaryCol
                            font.pixelSize: 64
                            opacity: 0.2
                            visible: npAlbumArtA.status !== Image.Ready && npAlbumArtB.status !== Image.Ready
                        }
                    }
                }

                // ── Right Column: Metadata + Controls ──
                Column {
                    width: parent.width * 0.62 - 40
                    height: parent.height
                    spacing: 12

                    Item { width: 1; height: 24 }

                    // Track title
                    Text {
                        width: parent.width
                        text: spotifyClient.trackTitle || "No Track"
                        color: ThemeValues.textCol
                        font.pixelSize: 24
                        font.family: ThemeValues.fontFamily
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                    }

                    // Artist
                    Text {
                        width: parent.width
                        text: spotifyClient.artist || ""
                        color: ThemeValues.primaryCol
                        font.pixelSize: 20
                        font.family: ThemeValues.fontFamily
                        elide: Text.ElideRight
                    }

                    // Album
                    Text {
                        width: parent.width
                        text: spotifyClient.album || ""
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                        font.pixelSize: 18
                        font.family: ThemeValues.fontFamily
                        elide: Text.ElideRight
                    }

                    // Explicit badge
                    Rectangle {
                        visible: spotifyClient.isExplicit
                        width: explicitText.width + 16
                        height: 24
                        radius: 4
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.15)

                        Text {
                            id: explicitText
                            anchors.centerIn: parent
                            text: "EXPLICIT"
                            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.6)
                            font.pixelSize: 11
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                        }
                    }

                    Item { width: 1; height: 8 }

                    // ── Progress Bar ──
                    Column {
                        width: parent.width
                        spacing: 6

                        // Seekable progress bar
                        Item {
                            width: parent.width
                            height: 20

                            // Background track
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 6
                                radius: 3
                                color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.15)

                                // Progress fill
                                Rectangle {
                                    width: spotifyClient.duration > 0
                                        ? parent.width * (spotifyClient.position / spotifyClient.duration)
                                        : 0
                                    height: parent.height
                                    radius: 3
                                    color: ThemeValues.primaryCol

                                    Behavior on width { NumberAnimation { duration: 200 } }
                                }
                            }

                            // Seek knob
                            Rectangle {
                                x: spotifyClient.duration > 0
                                    ? (parent.width - width) * (spotifyClient.position / spotifyClient.duration)
                                    : 0
                                anchors.verticalCenter: parent.verticalCenter
                                width: 16; height: 16
                                radius: 8
                                color: ThemeValues.primaryCol
                                visible: spotifyClient.isPlaying || spotifyClient.position > 0

                                Behavior on x { NumberAnimation { duration: 200 } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: function(mouse) {
                                    if (spotifyClient.duration > 0) {
                                        var ratio = mouse.x / width
                                        spotifyClient.seekTo(ratio * spotifyClient.duration)
                                    }
                                }
                            }
                        }

                        // Time stamps
                        Row {
                            width: parent.width

                            Text {
                                text: formatMs(spotifyClient.position)
                                color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.6)
                                font.pixelSize: 16
                                font.family: ThemeValues.fontFamily
                            }

                            Item { width: parent.width - 100; height: 1 }

                            Text {
                                text: formatMs(spotifyClient.duration)
                                color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.6)
                                font.pixelSize: 16
                                font.family: ThemeValues.fontFamily
                            }
                        }
                    }

                    Item { width: 1; height: 4 }

                    // ── Transport Controls ──
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 24

                        // Shuffle
                        Rectangle {
                            width: 64; height: 64
                            radius: 32
                            color: "transparent"

                            Canvas {
                                anchors.centerIn: parent
                                width: 24; height: 24
                                property bool active: spotifyClient.shuffleEnabled
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = (active ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.4)).toString()
                                    ctx.lineWidth = 2; ctx.lineCap = "round"
                                    ctx.beginPath(); ctx.moveTo(2, 6); ctx.lineTo(22, 18); ctx.stroke()
                                    ctx.beginPath(); ctx.moveTo(2, 18); ctx.lineTo(22, 6); ctx.stroke()
                                    ctx.beginPath(); ctx.moveTo(18, 4); ctx.lineTo(22, 6); ctx.lineTo(18, 8); ctx.stroke()
                                    ctx.beginPath(); ctx.moveTo(18, 16); ctx.lineTo(22, 18); ctx.lineTo(18, 20); ctx.stroke()
                                }
                                onActiveChanged: requestPaint()
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: spotifyClient.toggleShuffle()
                            }
                        }

                        // Previous
                        Rectangle {
                            width: 76; height: 76
                            radius: 38
                            color: "transparent"
                            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.5)
                            border.width: 2

                            Image {
                                id: spPrevIcon
                                anchors.centerIn: parent
                                source: iconSource("previous")
                                width: 24; height: 24
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                visible: status === Image.Ready
                                layer.enabled: true
                                layer.effect: ColorOverlay { color: ThemeValues.primaryCol }
                            }

                            Canvas {
                                anchors.centerIn: parent
                                width: 24; height: 24
                                visible: spPrevIcon.status !== Image.Ready
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = ThemeValues.primaryCol.toString()
                                    ctx.fillRect(2, 4, 3, 16)
                                    ctx.beginPath(); ctx.moveTo(20, 4); ctx.lineTo(7, 12); ctx.lineTo(20, 20); ctx.closePath(); ctx.fill()
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: spotifyClient.previous()
                            }
                        }

                        // Play/Pause
                        Rectangle {
                            width: 96; height: 96
                            radius: 48
                            color: ThemeValues.primaryCol

                            Canvas {
                                anchors.centerIn: parent
                                width: 36; height: 36
                                property bool playing: spotifyClient.isPlaying
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = ThemeValues.bgCol.toString()
                                    if (playing) {
                                        ctx.fillRect(8, 4, 7, 28)
                                        ctx.fillRect(21, 4, 7, 28)
                                    } else {
                                        ctx.beginPath(); ctx.moveTo(8, 4); ctx.lineTo(30, 18); ctx.lineTo(8, 32); ctx.closePath(); ctx.fill()
                                    }
                                }
                                onPlayingChanged: requestPaint()
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: spotifyClient.isPlaying ? spotifyClient.pause() : spotifyClient.resume()
                            }
                        }

                        // Next
                        Rectangle {
                            width: 76; height: 76
                            radius: 38
                            color: "transparent"
                            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.5)
                            border.width: 2

                            Canvas {
                                anchors.centerIn: parent
                                width: 24; height: 24
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = ThemeValues.primaryCol.toString()
                                    ctx.beginPath(); ctx.moveTo(4, 4); ctx.lineTo(17, 12); ctx.lineTo(4, 20); ctx.closePath(); ctx.fill()
                                    ctx.fillRect(19, 4, 3, 16)
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: spotifyClient.next()
                            }
                        }

                        // Repeat
                        Rectangle {
                            width: 64; height: 64
                            radius: 32
                            color: "transparent"

                            Canvas {
                                anchors.centerIn: parent
                                width: 24; height: 24
                                property int rMode: spotifyClient.repeatMode
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height)
                                    var col = rMode > 0 ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.4)
                                    ctx.strokeStyle = col.toString(); ctx.fillStyle = col.toString()
                                    ctx.lineWidth = 2; ctx.lineCap = "round"; ctx.lineJoin = "round"
                                    ctx.beginPath()
                                    ctx.moveTo(6, 8); ctx.lineTo(18, 8); ctx.arc(18, 12, 4, -Math.PI/2, Math.PI/2)
                                    ctx.lineTo(6, 16); ctx.arc(6, 12, 4, Math.PI/2, -Math.PI/2, true)
                                    ctx.stroke()
                                    ctx.beginPath(); ctx.moveTo(16, 5); ctx.lineTo(20, 8); ctx.lineTo(16, 11); ctx.stroke()
                                    if (rMode === 2) {
                                        ctx.font = "bold 9px sans-serif"
                                        ctx.fillText("1", 10, 15)
                                    }
                                }
                                onRModeChanged: requestPaint()
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: spotifyClient.cycleRepeatMode()
                            }
                        }
                    }

                    // ── Bottom Row: Favorite + Queue ──
                    Row {
                        width: parent.width
                        spacing: 12

                        // Favorite button
                        Rectangle {
                            width: 44; height: 44
                            radius: ThemeValues.radius
                            color: favMa.pressed
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                                : "transparent"
                            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "♡"
                                color: ThemeValues.primaryCol
                                font.pixelSize: 22
                            }

                            MouseArea {
                                id: favMa
                                anchors.fill: parent
                                onClicked: {
                                    var trackModel = spotifyClient.queue[spotifyClient.queuePosition]
                                    if (trackModel && trackModel.id) {
                                        spotifyClient.addFavorite(trackModel.id)
                                    }
                                }
                            }
                        }

                        Item { width: parent.width - 44 - queueBtn.width - 12; height: 1 }

                        Rectangle {
                            id: queueBtn
                            width: 120; height: 44
                            radius: ThemeValues.radius
                            color: queueBtnMa.pressed
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                                : "transparent"
                            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                            border.width: 1

                            Row {
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    text: "Queue"
                                    color: ThemeValues.textCol
                                    font.pixelSize: ThemeValues.fontSize - 2
                                    font.family: ThemeValues.fontFamily
                                }

                                Text {
                                    text: "☰"
                                    color: ThemeValues.primaryCol
                                    font.pixelSize: 18
                                }
                            }

                            MouseArea {
                                id: queueBtnMa
                                anchors.fill: parent
                                onClicked: queueVisible = !queueVisible
                            }
                        }
                    }
                }
            }

            // ── Queue Panel (slides in from right) ──
            Rectangle {
                id: queuePanel
                width: 400
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                x: queueVisible ? parent.width - width : parent.width
                z: 20
                color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.97)
                border.color: ThemeValues.primaryCol
                border.width: 1

                Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    // Queue header
                    Row {
                        width: parent.width
                        height: 44

                        Text {
                            text: "QUEUE"
                            color: ThemeValues.primaryCol
                            font.pixelSize: ThemeValues.fontSize + 2
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Item { width: parent.width - 140; height: 1 }

                        Rectangle {
                            width: 44; height: 44
                            radius: ThemeValues.radius
                            color: closeQueueMa.pressed
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                                : "transparent"
                            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                            border.width: 1
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "×"
                                color: ThemeValues.textCol
                                font.pixelSize: 22
                                font.family: ThemeValues.fontFamily
                            }

                            MouseArea {
                                id: closeQueueMa
                                anchors.fill: parent
                                onClicked: queueVisible = false
                            }
                        }
                    }

                    // Queue list
                    ListView {
                        id: queueList
                        width: parent.width
                        height: parent.height - 110
                        clip: true
                        spacing: 4
                        model: spotifyClient.queue

                        delegate: Rectangle {
                            width: queueList.width
                            height: 68
                            radius: ThemeValues.radius
                            property bool isCurrent: index === spotifyClient.queuePosition
                            color: isCurrent
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                                : Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.2)
                            border.color: isCurrent ? ThemeValues.primaryCol
                                : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.1)
                            border.width: isCurrent ? 1 : 0

                            Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 10

                                // Small art
                                Rectangle {
                                    width: 48; height: 48
                                    radius: 4
                                    color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                                    anchors.verticalCenter: parent.verticalCenter

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 1
                                        source: modelData.image_url || ""
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                    }
                                }

                                Column {
                                    width: parent.width - 58
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 3

                                    Text {
                                        width: parent.width
                                        text: modelData.title || ""
                                        color: isCurrent ? ThemeValues.primaryCol : ThemeValues.textCol
                                        font.pixelSize: ThemeValues.fontSize - 2
                                        font.family: ThemeValues.fontFamily
                                        font.weight: isCurrent ? Font.Bold : Font.Normal
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        width: parent.width
                                        text: modelData.artist || ""
                                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                                        font.pixelSize: ThemeValues.fontSize - 4
                                        font.family: ThemeValues.fontFamily
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (modelData.id) {
                                        spotifyClient.playTrackInContext(modelData.id, spotifyClient.queue, index)
                                    }
                                }
                            }
                        }

                        section.property: ""
                    }

                    // Clear queue button
                    Rectangle {
                        width: parent.width
                        height: 44
                        radius: ThemeValues.radius
                        color: clearQMa.pressed
                            ? Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.2)
                            : "transparent"
                        border.color: Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.4)
                        border.width: 1
                        visible: spotifyClient.queue.length > 0

                        Text {
                            anchors.centerIn: parent
                            text: "Clear Queue"
                            color: ThemeValues.errorCol
                            font.pixelSize: ThemeValues.fontSize - 2
                            font.family: ThemeValues.fontFamily
                        }

                        MouseArea {
                            id: clearQMa
                            anchors.fill: parent
                            onClicked: {
                                spotifyClient.clearQueue()
                                spotifyClient.stop()
                                queueVisible = false
                                currentView = "search"
                            }
                        }
                    }
                }
            }
        }
    }

    // Album/Playlist header component for detail view
    Component {
        id: albumHeaderComponent

        Item {
            width: trackList.width
            height: 160

            Row {
                anchors.fill: parent
                anchors.bottomMargin: 12
                spacing: 16

                // Album art
                Rectangle {
                    width: 140; height: 140
                    radius: 8
                    color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.6)
                    border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                    border.width: 1

                    Image {
                        anchors.fill: parent
                        anchors.margins: 2
                        source: browseData ? (browseData.image_url || "") : ""
                        fillMode: Image.PreserveAspectCrop
                        visible: status === Image.Ready
                    }
                }

                Column {
                    width: parent.width - 156
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6

                    Text {
                        width: parent.width
                        text: browseData ? (browseData.title || browseData.name || "") : ""
                        color: ThemeValues.textCol
                        font.pixelSize: ThemeValues.fontSize + 4
                        font.family: ThemeValues.fontFamily
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }

                    Text {
                        text: browseData ? (browseData.artist || browseData.owner || "") : ""
                        color: ThemeValues.primaryCol
                        font.pixelSize: ThemeValues.fontSize
                        font.family: ThemeValues.fontFamily
                    }

                    Text {
                        text: {
                            if (!browseData) return ""
                            var parts = []
                            if (browseData.year) parts.push(browseData.year)
                            if (browseData.num_tracks) parts.push(browseData.num_tracks + " tracks")
                            return parts.join(" · ")
                        }
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                        font.pixelSize: ThemeValues.fontSize - 3
                        font.family: ThemeValues.fontFamily
                    }

                    Item { width: 1; height: 4 }

                    Row {
                        spacing: 12

                        Rectangle {
                            width: 120; height: 40
                            radius: ThemeValues.radius
                            color: playAllMa.pressed
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                                : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                            border.color: ThemeValues.primaryCol
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "▶  Play All"
                                color: ThemeValues.primaryCol
                                font.pixelSize: ThemeValues.fontSize - 2
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                            }

                            MouseArea {
                                id: playAllMa
                                anchors.fill: parent
                                onClicked: {
                                    if (browseTracks.length > 0) {
                                        var firstTrack = browseTracks[0]
                                        spotifyClient.playTrackInContext(firstTrack.id, browseTracks, 0)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            width: 110; height: 40
                            radius: ThemeValues.radius
                            color: shuffleAllMa.pressed
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                                : "transparent"
                            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "⇄  Shuffle"
                                color: ThemeValues.textCol
                                font.pixelSize: ThemeValues.fontSize - 2
                                font.family: ThemeValues.fontFamily
                            }

                            MouseArea {
                                id: shuffleAllMa
                                anchors.fill: parent
                                onClicked: {
                                    if (browseTracks.length > 0) {
                                        if (!spotifyClient.shuffleEnabled) spotifyClient.toggleShuffle()
                                        var firstTrack = browseTracks[0]
                                        spotifyClient.playTrackInContext(firstTrack.id, browseTracks, 0)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        console.log("Spotify screen loaded")
        if (!spotifyClient.isConnected) {
            spotifyClient.startService()
        } else if (spotifyClient.isLoggedIn) {
            spotifyClient.getFavorites()
        }
    }
}
