import QtQuick 2.15
import QtQuick.Effects
import HeadUnit

Item {
    id: root
    anchors.fill: parent
    property var theme: null
    property var bluetoothManager: null

    signal messageFromJs(string cmd, var payload)

    property int libraryTab: 0
    property var libraryModel: []


    Connections {
        target: mediaController
        function onPlayStateChanged() {
            root.messageFromJs("playbackEvent", {
                event: mediaController.isPlaying ? "playing" : "paused"
            })
        }
        function onPlaylistsReceived(playlists) {
            if (libraryTab === 0) libraryModel = playlists
        }
        function onArtistsReceived(artists) {
            if (libraryTab === 1) libraryModel = artists
        }
        function onAlbumsReceived(albums) {
            if (libraryTab === 2) libraryModel = albums
        }
    }

    property bool showLibrary: false

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        Row {
            anchors.centerIn: parent
            width: parent.width - 40
            height: parent.height - 40
            spacing: 20

            // Left: Album Art
            Rectangle {
                width: showLibrary ? 0 : 300
                height: parent.height
                visible: !showLibrary
                color: "transparent"

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 220
                        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.5)
                        border.color: ThemeValues.primaryCol
                        border.width: 2
                        radius: 10

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowBlur: 0.5
                            shadowOpacity: 0.7
                            shadowColor: ThemeValues.primaryCol
                        }

                        Image {
                            id: albumArt
                            anchors.fill: parent
                            anchors.margins: 2
                            source: mediaController.albumArtUrl || ""
                            fillMode: Image.PreserveAspectCrop
                            smooth: true
                            visible: status === Image.Ready
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "♪"
                            font.pixelSize: 80
                            color: ThemeValues.primaryCol
                            opacity: 0.3
                            visible: albumArt.status !== Image.Ready
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 180
                        height: 24
                        color: "transparent"
                        border.color: ThemeValues.primaryCol
                        border.width: 1
                        radius: 12
                        visible: mediaController.isConnected

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            Rectangle {
                                width: 5
                                height: 5
                                radius: 2.5
                                color: ThemeValues.primaryCol
                                anchors.verticalCenter: parent.verticalCenter
                                visible: mediaController.isPlaying

                                SequentialAnimation on opacity {
                                    running: mediaController.isPlaying
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 600 }
                                    NumberAnimation { to: 1.0; duration: 600 }
                                }
                            }

                            Text {
                                text: {
                                    var source = mediaController.activeApp ? mediaController.activeApp.toUpperCase() : "";
                                    var status = mediaController.isPlaying ? "PLAYING" : "PAUSED";
                                    return source ? source + " • " + status : status;
                                }
                                color: ThemeValues.primaryCol
                                font.pixelSize: 10
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

            // Right: Track Info + Controls
            Item {
                width: parent.width - 320
                height: parent.height

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    spacing: 8

                Row {
                    width: parent.width
                    spacing: 8

                    Column {
                        width: parent.width - 80
                        spacing: 4

                        Text {
                            width: parent.width
                            text: mediaController.trackTitle || "No Track Playing"
                            color: ThemeValues.textCol
                            font.pixelSize: ThemeValues.fontSize + 4
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: mediaController.artist || "Unknown Artist"
                            color: ThemeValues.primaryCol
                            font.pixelSize: ThemeValues.fontSize
                            font.family: ThemeValues.fontFamily
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: mediaController.album || ""
                            color: ThemeValues.textCol
                            font.pixelSize: ThemeValues.fontSize - 2
                            font.family: ThemeValues.fontFamily
                            opacity: 0.6
                            elide: Text.ElideRight
                            visible: mediaController.album !== ""
                        }

                        // Source app badge
                        Rectangle {
                            width: sourceText.width + 16
                            height: 20
                            color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.4)
                            border.color: ThemeValues.primaryCol
                            border.width: 1
                            radius: 10
                            visible: mediaController.isConnected && mediaController.activeApp !== ""

                            Text {
                                id: sourceText
                                anchors.centerIn: parent
                                text: "via " + (mediaController.activeApp || "Phone")
                                color: ThemeValues.primaryCol
                                font.pixelSize: ThemeValues.fontSize - 5
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Medium
                            }
                        }
                    }

                    Column {
                        spacing: 6

                        ActionIcon {
                            iconText: "☰"
                            isActive: showLibrary
                            onClicked: {
                                showLibrary = !showLibrary
                                if (showLibrary) {
                                    mediaController.requestPlaylists()
                                }
                            }
                        }

                        ActionIcon {
                            iconText: "⇄"
                            isActive: mediaController.shuffleEnabled
                            onClicked: mediaController.toggleShuffle()
                        }

                        ActionIcon {
                            iconText: "↻"
                            isActive: mediaController.repeatMode !== 0
                            onClicked: mediaController.cycleRepeatMode()
                        }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 6

                    Row {
                        width: parent.width

                        Text {
                            text: formatTime(mediaController.trackPosition)
                            color: ThemeValues.textCol
                            font.pixelSize: ThemeValues.fontSize - 3
                            font.family: ThemeValues.fontFamily
                            opacity: 0.7
                        }

                        Item { width: parent.width - 120; height: 1 }

                        Text {
                            text: formatTime(mediaController.trackDuration)
                            color: ThemeValues.textCol
                            font.pixelSize: ThemeValues.fontSize - 3
                            font.family: ThemeValues.fontFamily
                            opacity: 0.7
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 8
                        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.5)
                        border.color: ThemeValues.primaryCol
                        border.width: 1
                        radius: 4

                        Rectangle {
                            width: mediaController.trackDuration > 0
                                   ? parent.width * (mediaController.trackPosition / mediaController.trackDuration)
                                   : 0
                            height: parent.height
                            color: ThemeValues.primaryCol
                            radius: 4
                            Behavior on width { NumberAnimation { duration: 200 } }
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: mediaController.isConnected && mediaController.trackDuration > 0
                            onClicked: {
                                var ratio = mouse.x / width
                                var newPos = Math.floor(ratio * mediaController.trackDuration)
                                mediaController.seekTo(newPos)
                            }
                        }
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 16

                    SkipButton {
                        text: "−15"
                        enabled: mediaController.isConnected
                        onClicked: mediaController.skipBackward(15)
                    }

                    ControlButton {
                        iconKey: "previous"
                        width: 96
                        height: 96
                        enabled: mediaController.isConnected
                        onClicked: mediaController.previous()
                    }

                    Rectangle {
                        width: 96
                        height: 96
                        color: "transparent"

                        Image {
                            anchors.centerIn: parent
                            source: root.theme && root.theme.iconPath
                                    ? root.theme.iconPath(mediaController.isPlaying ? "pause" : "play")
                                    : ""
                            width: 56
                            height: 56
                            fillMode: Image.PreserveAspectFit
                            smooth: true

                            layer.enabled: true
                            layer.effect: MultiEffect {
                                colorization: 1.0
                                colorizationColor: mediaController.isPlaying ?
                                    ThemeValues.primaryCol : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.7)
                            }
                        }

                        transform: Scale {
                            id: playScale
                            origin.x: parent.width / 2
                            origin.y: parent.height / 2
                            xScale: 1; yScale: 1
                            Behavior on xScale { NumberAnimation { duration: 100 } }
                            Behavior on yScale { NumberAnimation { duration: 100 } }
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: mediaController.isConnected
                            onPressed: { playScale.xScale = 0.88; playScale.yScale = 0.88 }
                            onReleased: { playScale.xScale = 1; playScale.yScale = 1 }
                            onClicked: mediaController.isPlaying ?
                                mediaController.pause() : mediaController.play()
                        }
                    }

                    ControlButton {
                        iconKey: "next"
                        width: 96
                        height: 96
                        enabled: mediaController.isConnected
                        onClicked: mediaController.next()
                    }

                    SkipButton {
                        text: "+15"
                        enabled: mediaController.isConnected
                        onClicked: mediaController.skipForward(15)
                    }
                }
                }
            }
        }

        // Library Panel (slides over album art area)
        Rectangle {
            visible: showLibrary
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 300
            color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.95)
            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                // Tab buttons
                Row {
                    width: parent.width
                    spacing: 4

                    Repeater {
                        model: ["Playlists", "Artists", "Albums"]

                        Rectangle {
                            width: (parent.width - 8) / 3
                            height: 32
                            radius: 6
                            color: libraryTab === index
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.25)
                                : "transparent"
                            border.color: libraryTab === index
                                ? ThemeValues.primaryCol
                                : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                            border.width: 1

                            property int libraryTab: root.libraryTab

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: parent.libraryTab === index ? ThemeValues.primaryCol : ThemeValues.textCol
                                font.pixelSize: ThemeValues.fontSize - 3
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.libraryTab = index
                                    if (index === 0) mediaController.requestPlaylists()
                                    else if (index === 1) mediaController.requestArtists()
                                    else mediaController.requestAlbums()
                                }
                            }
                        }
                    }
                }

                // Library list
                ListView {
                    id: libraryList
                    width: parent.width
                    height: parent.height - 44
                    clip: true
                    spacing: 4
                    model: libraryModel

                    delegate: Rectangle {
                        width: libraryList.width
                        height: 48
                        color: libItemMa.pressed
                            ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.05)
                        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                        border.width: 1
                        radius: 6

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            text: modelData.name || modelData.title || "Unknown"
                            color: ThemeValues.textCol
                            font.pixelSize: ThemeValues.fontSize - 1
                            font.family: ThemeValues.fontFamily
                            elide: Text.ElideRight
                        }

                        MouseArea {
                            id: libItemMa
                            anchors.fill: parent
                            onClicked: {
                                if (modelData.id) {
                                    mediaController.playPlaylist(modelData.id)
                                }
                            }
                        }
                    }

                    // Empty state
                    Text {
                        visible: libraryList.count === 0
                        anchors.centerIn: parent
                        text: mediaController.isConnected
                            ? "No items found"
                            : "Connect a phone to browse"
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.4)
                        font.pixelSize: ThemeValues.fontSize - 1
                        font.family: ThemeValues.fontFamily
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        // Connection Status
        Text {
            visible: !mediaController.isConnected
            anchors.centerIn: parent
            text: "Connect a phone via Bluetooth to control music"
            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
            font.pixelSize: ThemeValues.fontSize
            font.family: ThemeValues.fontFamily
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
        opacity: enabled ? 1.0 : 0.4

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
                colorizationColor: ThemeValues.primaryCol
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
            enabled: btn.enabled
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
            color: parent.isActive ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
            font.pixelSize: ThemeValues.fontSize + 12
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

    // Skip Button Component (text-based, compact)
    component SkipButton: Rectangle {
        width: 48
        height: 48
        color: "transparent"
        opacity: enabled ? 1.0 : 0.3

        property string text: ""
        signal clicked()

        anchors.verticalCenter: parent.verticalCenter

        Text {
            anchors.centerIn: parent
            text: parent.text
            color: ThemeValues.primaryCol
            font.pixelSize: ThemeValues.fontSize - 2
            font.family: ThemeValues.fontFamily
            font.weight: Font.Bold
            opacity: 0.7
        }

        transform: Scale {
            id: skipScale
            origin.x: parent.width / 2
            origin.y: parent.height / 2
            xScale: 1; yScale: 1
            Behavior on xScale { NumberAnimation { duration: 100 } }
            Behavior on yScale { NumberAnimation { duration: 100 } }
        }

        MouseArea {
            anchors.fill: parent
            enabled: parent.enabled
            onPressed: { skipScale.xScale = 0.85; skipScale.yScale = 0.85 }
            onReleased: { skipScale.xScale = 1; skipScale.yScale = 1 }
            onClicked: parent.clicked()
        }
    }

    Component.onCompleted: {
        console.log("Music screen loaded")

        // Auto-connect to the first connected Bluetooth device
        if (!mediaController.isConnected && bluetoothManager) {
            var connectedAddress = bluetoothManager.getFirstConnectedDeviceAddress()

            if (connectedAddress !== "") {
                console.log("Music: Auto-connecting to device:", connectedAddress)
                mediaController.connectToDevice(connectedAddress)
            } else {
                console.log("Music: No connected Bluetooth device found")
            }
        }
    }
}
