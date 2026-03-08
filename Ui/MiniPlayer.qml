import QtQuick 2.15
import Qt5Compat.GraphicalEffects

Rectangle {
    id: root
    required property var theme

    // Height states
    readonly property int miniHeight: 90
    readonly property int expandedHeight: 400
    property bool expanded: false

    height: expanded ? expandedHeight : miniHeight
    color: Qt.rgba(0, 0, 0, 0.95)
    border.color: theme?.palette?.primary ?? "#00f0ff"
    border.width: 2

    Behavior on height {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    // Theme properties
    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Orbitron"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 24

    // Media info from mediaController
    readonly property string currentTrack: mediaController ? mediaController.trackTitle : "Not Playing"
    readonly property string currentArtist: mediaController ? mediaController.artist : ""
    readonly property string currentAlbum: mediaController ? mediaController.album : ""
    readonly property bool isPlaying: mediaController ? mediaController.isPlaying : false
    readonly property bool isConnected: mediaController ? mediaController.isConnected : false
    readonly property string appName: mediaController ? mediaController.activeApp : "Music"

    // Collapsed mini view
    Item {
        id: miniView
        anchors.fill: parent
        visible: !root.expanded

        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = true
        }

        Row {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // Album art placeholder
            Rectangle {
                width: 66
                height: 66
                radius: 8
                color: Qt.rgba(0, 0, 0, 0.5)
                border.color: root.primaryCol
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "🎵"
                    font.pixelSize: 32
                    color: root.primaryCol
                }
            }

            // Track info
            Column {
                width: parent.width - 66 - 200 - 24
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    width: parent.width
                    text: root.currentTrack
                    color: root.textCol
                    font.family: root.fontFamily
                    font.pixelSize: root.fontSize - 2
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: root.currentArtist + (root.currentAlbum ? " • " + root.currentAlbum : "")
                    color: root.textCol
                    font.family: root.fontFamily
                    font.pixelSize: root.fontSize - 8
                    opacity: 0.7
                    elide: Text.ElideRight
                }
            }

            // Controls
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 20

                Rectangle {
                    width: 52
                    height: 52
                    radius: 26
                    color: "transparent"
                    border.color: root.primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: root.isPlaying ? "⏸" : "▶"
                        color: root.primaryCol
                        font.pixelSize: 24
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (mediaController) {
                                if (root.isPlaying) {
                                    mediaController.pause()
                                } else {
                                    mediaController.play()
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: 52
                    height: 52
                    radius: 26
                    color: "transparent"
                    border.color: root.primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "⏭"
                        color: root.primaryCol
                        font.pixelSize: 24
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (mediaController) {
                                mediaController.next()
                            }
                        }
                    }
                }
            }
        }
    }

    // Expanded view
    Item {
        id: expandedView
        anchors.fill: parent
        visible: root.expanded

        Column {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 20

            // Header with close button
            Row {
                width: parent.width
                height: 40

                Text {
                    text: root.appName
                    color: root.primaryCol
                    font.family: root.fontFamily
                    font.pixelSize: root.fontSize + 4
                    font.weight: Font.Bold
                }

                Item { width: parent.width - 200; height: 1 }

                Rectangle {
                    width: 40
                    height: 40
                    radius: 20
                    color: "transparent"
                    border.color: root.primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: root.primaryCol
                        font.pixelSize: 28
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.expanded = false
                    }
                }
            }

            // Album art (large)
            Rectangle {
                width: 200
                height: 200
                anchors.horizontalCenter: parent.horizontalCenter
                radius: 12
                color: Qt.rgba(0, 0, 0, 0.5)
                border.color: root.primaryCol
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "🎵"
                    font.pixelSize: 80
                    color: root.primaryCol
                }
            }

            // Track info (detailed)
            Column {
                width: parent.width
                spacing: 8

                Text {
                    width: parent.width
                    text: root.currentTrack
                    color: root.textCol
                    font.family: root.fontFamily
                    font.pixelSize: root.fontSize + 2
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: root.currentArtist
                    color: root.textCol
                    font.family: root.fontFamily
                    font.pixelSize: root.fontSize - 2
                    opacity: 0.8
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: root.currentAlbum
                    color: root.textCol
                    font.family: root.fontFamily
                    font.pixelSize: root.fontSize - 4
                    opacity: 0.6
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Item { height: 10 }

            // Playback controls (large)
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 30

                // Previous
                Rectangle {
                    width: 60
                    height: 60
                    radius: 30
                    color: "transparent"
                    border.color: root.primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "⏮"
                        color: root.primaryCol
                        font.pixelSize: 28
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (mediaController) {
                                mediaController.previous()
                            }
                        }
                    }
                }

                // Play/Pause
                Rectangle {
                    width: 80
                    height: 80
                    radius: 40
                    color: root.primaryCol

                    Text {
                        anchors.centerIn: parent
                        text: root.isPlaying ? "⏸" : "▶"
                        color: root.bgCol
                        font.pixelSize: 36
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (mediaController) {
                                if (root.isPlaying) {
                                    mediaController.pause()
                                } else {
                                    mediaController.play()
                                }
                            }
                        }
                    }
                }

                // Next
                Rectangle {
                    width: 60
                    height: 60
                    radius: 30
                    color: "transparent"
                    border.color: root.primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "⏭"
                        color: root.primaryCol
                        font.pixelSize: 28
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (mediaController) {
                                mediaController.next()
                            }
                        }
                    }
                }
            }
        }

        // Swipe down to collapse
        MouseArea {
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            height: 50

            property real startY: 0

            onPressed: startY = mouse.y
            onReleased: {
                if (mouse.y - startY > 30) {
                    root.expanded = false
                }
            }
        }
    }

    // Top border glow effect
    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
        }
        height: 2
        color: root.primaryCol
        opacity: 0.6

        layer.enabled: true
        layer.effect: Glow {
            samples: 15
            color: root.primaryCol
            spread: 0.5
        }
    }
}
