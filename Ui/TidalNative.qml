import QtQuick 2.15
import QtQuick.Effects
import QtQuick.Controls 2.15

Item {
    id: root
    anchors.fill: parent
    required property var theme

    signal messageFromJs(string cmd, var payload)

    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16

    property int currentView: 0 // 0=NowPlaying, 1=Search, 2=Library, 3=Queue

    Connections {
        target: tidalController
        function onPlayStateChanged() {
            root.messageFromJs("playbackEvent", {
                event: tidalController.isPlaying ? "playing" : "paused"
            })
        }
    }

    Rectangle {
        anchors.fill: parent
        color: bgCol

        Item {
            id: contentArea
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                bottom: bottomNav.top
                margins: 12
            }

            // NOW PLAYING VIEW
            Item {
                id: nowPlayingView
                anchors.fill: parent
                visible: currentView === 0
                opacity: currentView === 0 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250 } }

                Row {
                    anchors.fill: parent
                    spacing: 24

                    // Album Art
                    Rectangle {
                        width: 200
                        height: 200
                        anchors.verticalCenter: parent.verticalCenter
                        color: Qt.rgba(0, 0, 0, 0.5)
                        border.color: primaryCol
                        border.width: 2
                        radius: 10

                        Image {
                            anchors.fill: parent
                            anchors.margins: 2
                            source: tidalController.albumArtUrl || ""
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "♪"
                            font.pixelSize: 80
                            color: primaryCol
                            opacity: 0.3
                            visible: !tidalController.albumArtUrl
                        }
                    }

                    // Track info + controls
                    Column {
                        width: parent.width - 240
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 24

                        Item { height: 20 } // Top spacer

                        // Track info
                        Column {
                            width: parent.width
                            spacing: 6

                            Text {
                                text: tidalController.currentTrack || "No Track"
                                color: textCol
                                font.pixelSize: fontSize + 10
                                font.family: fontFamily
                                font.weight: Font.Bold
                                width: parent.width
                                elide: Text.ElideRight
                            }

                            Text {
                                text: tidalController.currentArtist || ""
                                color: primaryCol
                                font.pixelSize: fontSize + 3
                                font.family: fontFamily
                                width: parent.width
                                elide: Text.ElideRight
                            }

                            Text {
                                text: tidalController.currentAlbum || ""
                                color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.4)
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                width: parent.width
                                elide: Text.ElideRight
                            }
                        }

                        // Progress bar
                        Row {
                            width: parent.width
                            spacing: 10

                            Text {
                                text: formatTime(tidalController.trackPosition)
                                color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                                font.pixelSize: fontSize - 2
                                font.family: fontFamily
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                width: parent.width - 110
                                height: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                                radius: 2

                                Rectangle {
                                    width: tidalController.trackDuration > 0 ?
                                        parent.width * (tidalController.trackPosition / tidalController.trackDuration) : 0
                                    height: parent.height
                                    color: primaryCol
                                    radius: 2
                                }
                            }

                            Text {
                                text: formatTime(tidalController.trackDuration)
                                color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                                font.pixelSize: fontSize - 2
                                font.family: fontFamily
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // Playback controls + action icons combined
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 24

                            ControlButton {
                                iconKey: "previous"
                                width: 96
                                height: 96
                                onClicked: tidalController.previous()
                            }

                            Rectangle {
                                width: 96
                                height: 96
                                color: "transparent"

                                Image {
                                    anchors.centerIn: parent
                                    source: root.theme && root.theme.iconPath
                                            ? root.theme.iconPath(tidalController.isPlaying ? "pause" : "play")
                                            : ""
                                    width: 56
                                    height: 56
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true

                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        colorization: 1.0
                                        colorizationColor: tidalController.isPlaying ?
                                            primaryCol : Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.7)
                                    }
                                }

                                transform: Scale {
                                    id: tidalPlayScale
                                    origin.x: 48
                                    origin.y: 48
                                    xScale: 1; yScale: 1
                                    Behavior on xScale { NumberAnimation { duration: 100 } }
                                    Behavior on yScale { NumberAnimation { duration: 100 } }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onPressed: { tidalPlayScale.xScale = 0.88; tidalPlayScale.yScale = 0.88 }
                                    onReleased: { tidalPlayScale.xScale = 1; tidalPlayScale.yScale = 1 }
                                    onClicked: tidalController.togglePlayPause()
                                }
                            }

                            ControlButton {
                                iconKey: "next"
                                width: 96
                                height: 96
                                onClicked: tidalController.next()
                            }

                            Rectangle {
                                width: 1
                                height: 60
                                color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            ActionIcon {
                                iconText: "♥"
                                isActive: false
                                anchors.verticalCenter: parent.verticalCenter
                                onClicked: console.log("Add to favorites")
                            }

                            ActionIcon {
                                iconText: "↓"
                                isActive: false
                                anchors.verticalCenter: parent.verticalCenter
                                onClicked: console.log("Download track")
                            }
                        }
                    }
                }
            }

            // SEARCH VIEW
            Item {
                id: searchView
                anchors.fill: parent
                visible: currentView === 1
                opacity: currentView === 1 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250 } }

                Column {
                    anchors.fill: parent
                    spacing: 12

                    Rectangle {
                        width: parent.width
                        height: 44
                        color: Qt.rgba(0, 0, 0, 0.5)
                        border.color: primaryCol
                        border.width: 2
                        radius: 6

                        Row {
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 8

                            Item {
                                width: 40
                                height: parent.height

                                Image {
                                    anchors.centerIn: parent
                                    source: root.theme && root.theme.iconPath
                                            ? root.theme.iconPath("search")
                                            : ""
                                    width: 20
                                    height: 20
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true

                                    layer.enabled: true
                                    layer.effect: MultiEffect {
                                        colorization: 1.0
                                        colorizationColor: primaryCol
                                    }
                                }
                            }

                            TextInput {
                                id: searchInput
                                width: parent.width - 100
                                anchors.verticalCenter: parent.verticalCenter
                                color: textCol
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                clip: true
                                selectByMouse: true

                                Text {
                                    visible: searchInput.text.length === 0
                                    text: "Search tracks..."
                                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.4)
                                    font: searchInput.font
                                }

                                Keys.onReturnPressed: {
                                    tidalController.search(searchInput.text)
                                }
                            }

                            Rectangle {
                                width: 44
                                height: parent.height
                                color: "transparent"

                                Text {
                                    anchors.centerIn: parent
                                    text: "⏎"
                                    color: primaryCol
                                    font.pixelSize: 18
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: tidalController.search(searchInput.text)
                                }
                            }
                        }
                    }

                    GridView {
                        width: parent.width
                        height: parent.height - 56
                        clip: true
                        cellWidth: width / 3
                        cellHeight: 100

                        model: tidalController.searchResults

                        delegate: TrackDelegate {
                            trackData: modelData
                            onPlayClicked: {
                                tidalController.playTrack(modelData.id)
                                currentView = 0
                            }
                            onAddToQueueClicked: {
                                tidalController.addToQueue(modelData)
                            }
                            onFavoriteClicked: {
                                if (tidalController.isFavorite(modelData.id)) {
                                    tidalController.removeFromFavorites(modelData.id)
                                } else {
                                    tidalController.addToFavorites(modelData.id)
                                }
                            }
                            onDownloadClicked: {
                                if (tidalController.isDownloaded(modelData.id)) {
                                    tidalController.removeDownload(modelData.id)
                                } else {
                                    tidalController.downloadTrack(modelData.id)
                                }
                            }
                        }
                    }
                }
            }

            // LIBRARY VIEW
            Item {
                id: libraryView
                anchors.fill: parent
                visible: currentView === 2
                opacity: currentView === 2 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250 } }

                property int subView: 0 // 0=Favorites, 1=Playlists, 2=Albums, 3=Downloads

                Column {
                    anchors.fill: parent
                    spacing: 12

                    Row {
                        width: parent.width
                        height: 40
                        spacing: 12

                        SubNavButton {
                            text: "Favorites"
                            isActive: libraryView.subView === 0
                            onClicked: {
                                libraryView.subView = 0
                                tidalController.loadFavorites()
                            }
                        }

                        SubNavButton {
                            text: "Downloads"
                            isActive: libraryView.subView === 3
                            onClicked: {
                                libraryView.subView = 3
                                tidalController.loadDownloads()
                            }
                        }
                    }

                    ListView {
                        width: parent.width
                        height: parent.height - 52
                        clip: true
                        spacing: 6

                        model: libraryView.subView === 0 ? tidalController.favorites : tidalController.downloads

                        delegate: TrackDelegate {
                            trackData: modelData
                            onPlayClicked: {
                                tidalController.playTrack(modelData.id)
                                currentView = 0
                            }
                            onAddToQueueClicked: {
                                tidalController.addToQueue(modelData)
                            }
                            onFavoriteClicked: {
                                if (tidalController.isFavorite(modelData.id)) {
                                    tidalController.removeFromFavorites(modelData.id)
                                } else {
                                    tidalController.addToFavorites(modelData.id)
                                }
                            }
                            onDownloadClicked: {
                                if (tidalController.isDownloaded(modelData.id)) {
                                    tidalController.removeDownload(modelData.id)
                                } else {
                                    tidalController.downloadTrack(modelData.id)
                                }
                            }
                        }
                    }
                }
            }

            // QUEUE VIEW
            Item {
                id: queueView
                anchors.fill: parent
                visible: currentView === 3
                opacity: currentView === 3 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 250 } }

                ListView {
                    anchors.fill: parent
                    clip: true
                    spacing: 6

                    model: tidalController.queue

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 70
                        color: Qt.rgba(0, 0, 0, index === tidalController.currentQueueIndex ? 0.5 : 0.3)
                        border.color: index === tidalController.currentQueueIndex ?
                            primaryCol : Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                        border.width: index === tidalController.currentQueueIndex ? 2 : 1
                        radius: 4

                        Row {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 12

                            Text {
                                text: (index + 1).toString()
                                color: textCol
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                opacity: 0.5
                                width: 30
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Column {
                                width: parent.width - 150
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                Text {
                                    text: modelData.title || ""
                                    color: textCol
                                    font.pixelSize: fontSize - 1
                                    font.family: fontFamily
                                    font.weight: Font.Bold
                                    elide: Text.ElideRight
                                    width: parent.width
                                }

                                Text {
                                    text: modelData.artist || ""
                                    color: primaryCol
                                    font.pixelSize: fontSize - 3
                                    font.family: fontFamily
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                            }

                            Row {
                                spacing: 8
                                anchors.verticalCenter: parent.verticalCenter

                                Rectangle {
                                    width: 36
                                    height: 36
                                    color: "transparent"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "▶"
                                        color: primaryCol
                                        font.pixelSize: fontSize
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            tidalController.playFromQueue(index)
                                            currentView = 0
                                        }
                                    }
                                }

                                Rectangle {
                                    width: 36
                                    height: 36
                                    color: "transparent"

                                    Text {
                                        anchors.centerIn: parent
                                        text: "✕"
                                        color: textCol
                                        font.pixelSize: fontSize + 2
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: tidalController.removeFromQueue(index)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Bottom Navigation
        Rectangle {
            id: bottomNav
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 48
            color: Qt.rgba(0, 0, 0, 0.95)
            border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 12

                NavButton {
                    iconKey: "music"
                    buttonText: "Playing"
                    isActive: currentView === 0
                    onClicked: currentView = 0
                    width: 130
                }

                NavButton {
                    iconKey: "search"
                    buttonText: "Search"
                    isActive: currentView === 1
                    onClicked: currentView = 1
                    width: 130
                }

                NavButton {
                    iconKey: "home"
                    buttonText: "Library"
                    isActive: currentView === 2
                    onClicked: {
                        currentView = 2
                        if (tidalController.favorites.length === 0) {
                            tidalController.loadFavorites()
                        }
                    }
                    width: 130
                }

                NavButton {
                    iconKey: "music"
                    buttonText: "Queue (" + tidalController.queue.length + ")"
                    isActive: currentView === 3
                    onClicked: currentView = 3
                    width: 150
                }
            }
        }
    }

    function formatTime(ms) {
        var totalSeconds = Math.floor(ms / 1000)
        var minutes = Math.floor(totalSeconds / 60)
        var seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    // Control Button Component (NO CIRCLES)
    component ControlButton: Rectangle {
        id: btn
        width: 64
        height: 64
        color: "transparent"

        property string iconKey: ""

        signal clicked()

        Image {
            anchors.centerIn: parent
            source: root.theme && root.theme.iconPath && btn.iconKey !== ""
                    ? root.theme.iconPath(btn.iconKey)
                    : ""
            width: 56
            height: 56
            fillMode: Image.PreserveAspectFit
            smooth: true

            layer.enabled: true
            layer.effect: MultiEffect {
                colorization: 1.0
                colorizationColor: root.primaryCol
            }
        }

        transform: Scale {
            id: btnScale
            origin.x: btn.width / 2
            origin.y: btn.height / 2
            xScale: 1; yScale: 1
            Behavior on xScale { NumberAnimation { duration: 100 } }
            Behavior on yScale { NumberAnimation { duration: 100 } }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: { btnScale.xScale = 0.88; btnScale.yScale = 0.88 }
            onReleased: { btnScale.xScale = 1; btnScale.yScale = 1 }
            onClicked: btn.clicked()
        }
    }

    // Action Icon Component (NO CIRCLES)
    component ActionIcon: Rectangle {
        width: 56
        height: 56
        color: "transparent"

        property string iconText: ""
        property bool isActive: false

        signal clicked()

        Text {
            anchors.centerIn: parent
            text: parent.iconText
            color: parent.isActive ? root.primaryCol : Qt.rgba(root.textCol.r, root.textCol.g, root.textCol.b, 0.5)
            font.pixelSize: root.fontSize + 12
            font.weight: Font.Bold
        }

        transform: Scale {
            id: iconScale
            origin.x: parent.width / 2
            origin.y: parent.height / 2
            xScale: 1; yScale: 1
            Behavior on xScale { NumberAnimation { duration: 100 } }
            Behavior on yScale { NumberAnimation { duration: 100 } }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: { iconScale.xScale = 0.85; iconScale.yScale = 0.85 }
            onReleased: { iconScale.xScale = 1; iconScale.yScale = 1 }
            onClicked: parent.clicked()
        }
    }

    // Nav Button Component
    component NavButton: Rectangle {
        id: navBtn
        height: 36
        color: "transparent"
        border.color: isActive ? root.primaryCol : Qt.rgba(root.primaryCol.r, root.primaryCol.g, root.primaryCol.b, 0.3)
        border.width: 1
        radius: 6

        property string iconKey: ""
        property string buttonText: ""
        property bool isActive: false

        signal clicked()

        Row {
            anchors.centerIn: parent
            spacing: 6

            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: root.theme && root.theme.iconPath && navBtn.iconKey !== ""
                        ? root.theme.iconPath(navBtn.iconKey)
                        : ""
                width: 18
                height: 18
                fillMode: Image.PreserveAspectFit
                smooth: true

                layer.enabled: true
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: navBtn.isActive ? root.primaryCol : root.textCol
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: navBtn.buttonText
                color: navBtn.isActive ? root.primaryCol : root.textCol
                font.pixelSize: root.fontSize - 4
                font.family: root.fontFamily
                font.weight: Font.Bold
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: navBtn.clicked()
        }
    }

    // Sub Nav Button Component
    component SubNavButton: Rectangle {
        width: 140
        height: 36
        color: "transparent"
        border.color: isActive ? root.primaryCol : Qt.rgba(root.primaryCol.r, root.primaryCol.g, root.primaryCol.b, 0.3)
        border.width: 2
        radius: 6

        property string text: ""
        property bool isActive: false

        signal clicked()

        Text {
            anchors.centerIn: parent
            text: parent.text
            color: parent.isActive ? root.primaryCol : root.textCol
            font.pixelSize: root.fontSize - 2
            font.family: root.fontFamily
            font.weight: Font.Bold
        }

        MouseArea {
            anchors.fill: parent
            onClicked: parent.clicked()
        }
    }

    // Track Delegate Component
    component TrackDelegate: Rectangle {
        width: GridView.view ? GridView.view.cellWidth - 6 : ListView.view.width
        height: GridView.view ? GridView.view.cellHeight - 6 : 70
        color: Qt.rgba(0, 0, 0, 0.3)
        border.color: Qt.rgba(root.primaryCol.r, root.primaryCol.g, root.primaryCol.b, 0.2)
        border.width: 1
        radius: 4

        property var trackData: null
        property bool showDownloadBadge: false

        signal playClicked()
        signal addToQueueClicked()
        signal favoriteClicked()
        signal downloadClicked()

        Row {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            Column {
                width: parent.width - 50
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    text: trackData ? trackData.title : ""
                    color: root.textCol
                    font.pixelSize: root.fontSize - 2
                    font.family: root.fontFamily
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    width: parent.width
                }

                Text {
                    text: trackData ? trackData.artist : ""
                    color: root.primaryCol
                    font.pixelSize: root.fontSize - 4
                    font.family: root.fontFamily
                    elide: Text.ElideRight
                    width: parent.width
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Row {
                    spacing: 4

                    ActionIcon {
                        iconText: "♥"
                        isActive: tidalController.isFavorite(trackData.id)
                        onClicked: favoriteClicked()
                    }

                    ActionIcon {
                        iconText: "▶"
                        isActive: false
                        onClicked: playClicked()
                    }
                }

                ActionIcon {
                    width: 100
                    height: 32
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        color: root.textCol
                        font.pixelSize: root.fontSize + 4
                    }

                    onClicked: addToQueueClicked()
                }
            }
        }
    }

    Component.onCompleted: {
        console.log("TidalNative with Full Downloads & Favorites loaded")
        tidalController.loadFavorites()
        tidalController.loadDownloads()
    }
}
