import QtQuick 2.15
import QtQuick.Effects

Item {
    id: root
    anchors.fill: parent
    property var theme: null

    signal messageFromJs(string cmd, var payload)

    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16

    Connections {
        target: mediaController
        function onPlayStateChanged() {
            root.messageFromJs("playbackEvent", {
                event: mediaController.isPlaying ? "playing" : "paused"
            })
        }
    }

    Rectangle {
        anchors.fill: parent
        color: bgCol

        Row {
            anchors.centerIn: parent
            width: parent.width - 40
            height: parent.height - 40
            spacing: 20

            // Left: Album Art
            Rectangle {
                width: 300
                height: parent.height
                color: "transparent"

                Column {
                    anchors.centerIn: parent
                    spacing: 12

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 220
                        height: 220
                        color: Qt.rgba(0, 0, 0, 0.5)
                        border.color: primaryCol
                        border.width: 2
                        radius: 10

                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowBlur: 0.5
                            shadowOpacity: 0.7
                            shadowColor: primaryCol
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
                            color: primaryCol
                            opacity: 0.3
                            visible: albumArt.status !== Image.Ready
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 120
                        height: 24
                        color: "transparent"
                        border.color: primaryCol
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
                                color: primaryCol
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
                                text: mediaController.isPlaying ? "NOW PLAYING" : "PAUSED"
                                color: primaryCol
                                font.pixelSize: 10
                                font.family: fontFamily
                                font.weight: Font.Bold
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

            // Right: Track Info + Controls
            Column {
                width: parent.width - 320
                height: parent.height
                spacing: 16

                Row {
                    width: parent.width
                    spacing: 8

                    Column {
                        width: parent.width - 80
                        spacing: 4

                        Text {
                            width: parent.width
                            text: mediaController.trackTitle || "No Track Playing"
                            color: textCol
                            font.pixelSize: fontSize + 4
                            font.family: fontFamily
                            font.weight: Font.Bold
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: mediaController.artist || "Unknown Artist"
                            color: primaryCol
                            font.pixelSize: fontSize
                            font.family: fontFamily
                            elide: Text.ElideRight
                        }

                        Text {
                            width: parent.width
                            text: mediaController.album || ""
                            color: textCol
                            font.pixelSize: fontSize - 2
                            font.family: fontFamily
                            opacity: 0.6
                            elide: Text.ElideRight
                            visible: mediaController.album !== ""
                        }
                    }

                    Column {
                        spacing: 6

                        ActionIcon {
                            iconText: "♥"
                            isActive: false
                            onClicked: console.log("Add to favorites")
                        }

                        ActionIcon {
                            iconText: "⇄"
                            isActive: mediaController.shuffleEnabled
                            onClicked: mediaController.toggleShuffle()
                        }

                        ActionIcon {
                            iconText: "🔁"
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
                            color: textCol
                            font.pixelSize: fontSize - 3
                            font.family: fontFamily
                            opacity: 0.7
                        }

                        Item { width: parent.width - 120; height: 1 }

                        Text {
                            text: formatTime(mediaController.trackDuration)
                            color: textCol
                            font.pixelSize: fontSize - 3
                            font.family: fontFamily
                            opacity: 0.7
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 8
                        color: Qt.rgba(0, 0, 0, 0.5)
                        border.color: primaryCol
                        border.width: 1
                        radius: 4

                        Rectangle {
                            width: mediaController.trackDuration > 0
                                   ? parent.width * (mediaController.trackPosition / mediaController.trackDuration)
                                   : 0
                            height: parent.height
                            color: primaryCol
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

                Item { height: 10 }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 24

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
                                    primaryCol : Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.7)
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
                }
            }
        }

        // Connection Status
        Text {
            visible: !mediaController.isConnected
            anchors.centerIn: parent
            text: "Connect a phone via Bluetooth to control music"
            color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
            font.pixelSize: fontSize
            font.family: fontFamily
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
                colorizationColor: primaryCol
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
            color: parent.isActive ? primaryCol : Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
            font.pixelSize: fontSize + 12
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

    Component.onCompleted: {
        console.log("Music screen loaded with Tidal-style layout")

        // Auto-connect in mock mode for testing
        if (!mediaController.isConnected) {
            mediaController.connectToDevice("80:96:98:C8:69:17")
        }
    }
}
