import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

Rectangle {
    id: root
    property var theme: null
    property string audioSource: "none"  // "tidal", "spotify", "music", "phone", "none"

    // Theme-driven dimensions
    readonly property int miniHeight: Number(theme?.miniPlayer?.height ?? 90)
    readonly property int expandedHeight: Number(theme?.miniPlayer?.expandedHeight ?? 400)
    readonly property int _borderW: Number(theme?.miniPlayer?.borderWidth ?? 2)
    readonly property int _artSize: Number(theme?.miniPlayer?.artSize ?? 66)
    readonly property int _artRadius: Number(theme?.miniPlayer?.artRadius ?? 8)
    readonly property int _ctrlSize: Number(theme?.miniPlayer?.controlSize ?? 52)
    readonly property int _ctrlSizeExp: Number(theme?.miniPlayer?.controlSizeExpanded ?? 60)
    readonly property int _playSize: Number(theme?.miniPlayer?.playButtonSize ?? 80)
    readonly property int _iconSize: Number(theme?.miniPlayer?.iconSize ?? 22)
    readonly property int _iconSizeExp: Number(theme?.miniPlayer?.iconSizeExpanded ?? 26)
    readonly property int _playIconSize: Number(theme?.miniPlayer?.playIconSize ?? 32)
    readonly property int _ctrlSpacing: Number(theme?.miniPlayer?.controlSpacing ?? 20)
    readonly property bool _useGlow: theme?.miniPlayer?.useGlow ?? true

    property bool expanded: false

    // ── Multi-source property bindings ──
    readonly property bool isTidal: audioSource === "tidal"
    readonly property bool isSpotify: audioSource === "spotify"
    readonly property bool isStreaming: isTidal || isSpotify

    readonly property string currentTrack:
        isTidal ? (tidalClient ? tidalClient.trackTitle || "" : "")
        : isSpotify ? (spotifyClient ? spotifyClient.trackTitle || "" : "")
                : (mediaController ? mediaController.trackTitle : "No Track")

    readonly property string currentArtist:
        isTidal ? (tidalClient ? tidalClient.artist || "" : "")
        : isSpotify ? (spotifyClient ? spotifyClient.artist || "" : "")
                : (mediaController ? mediaController.artist : "")

    readonly property string currentAlbum:
        isTidal ? (tidalClient ? tidalClient.album || "" : "")
        : isSpotify ? (spotifyClient ? spotifyClient.album || "" : "")
                : (mediaController ? mediaController.album : "")

    readonly property bool isPlaying:
        isTidal ? (tidalClient ? tidalClient.isPlaying : false)
        : isSpotify ? (spotifyClient ? spotifyClient.isPlaying : false)
                : (mediaController ? mediaController.isPlaying : false)

    readonly property string albumArtUrl:
        isTidal ? (tidalClient ? tidalClient.albumArtUrl || "" : "")
        : isSpotify ? (spotifyClient ? spotifyClient.albumArtUrl || "" : "")
                : (mediaController ? mediaController.albumArtUrl.toString() : "")

    readonly property string sourceName:
        isTidal ? "TIDAL"
        : isSpotify ? "SPOTIFY"
        : (mediaController ? mediaController.activeApp : "Music")

    // Control forwarding
    function doPlay()     { isTidal ? tidalClient.resume() : isSpotify ? spotifyClient.resume() : mediaController.play() }
    function doPause()    { isTidal ? tidalClient.pause() : isSpotify ? spotifyClient.pause() : mediaController.pause() }
    function doNext()     { isTidal ? tidalClient.next() : isSpotify ? spotifyClient.next() : mediaController.next() }
    function doPrevious() { isTidal ? tidalClient.previous() : isSpotify ? spotifyClient.previous() : mediaController.previous() }

    height: expanded ? expandedHeight : miniHeight
    color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b,
                   Number(theme?.miniPlayer?.bgOpacity ?? 0.95))
    border.color: ThemeValues.primaryCol
    border.width: _borderW

    Behavior on height {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    // Helper to get icon paths
    function iconSource(key) {
        return (theme && theme.iconPath) ? theme.iconPath(key) : ""
    }

    // ── Collapsed mini view ──
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
            anchors.margins: ThemeValues.gap
            spacing: ThemeValues.gap

            // Album art
            Rectangle {
                width: root._artSize
                height: root._artSize
                radius: root._artRadius
                color: ThemeValues.cardBgCol
                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                border.width: 1

                Image {
                    id: miniArt
                    anchors.fill: parent
                    anchors.margins: 1
                    source: root.albumArtUrl
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                    smooth: true
                }

                // Fallback icon
                Image {
                    anchors.centerIn: parent
                    source: iconSource("music")
                    width: parent.width * 0.5
                    height: width
                    fillMode: Image.PreserveAspectFit
                    opacity: 0.4
                    visible: miniArt.status !== Image.Ready
                }
            }

            // Track info + source badge
            Column {
                width: parent.width - root._artSize - controlsMini.width - ThemeValues.gap * 2
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Text {
                    width: parent.width
                    text: root.currentTrack || "No Track"
                    color: ThemeValues.textCol
                    font.family: ThemeValues.fontFamily
                    font.pixelSize: ThemeValues.fontSize - 2
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: root.currentArtist + (root.currentAlbum ? " · " + root.currentAlbum : "")
                    color: ThemeValues.textCol
                    font.family: ThemeValues.fontFamily
                    font.pixelSize: ThemeValues.fontSize - 6
                    opacity: 0.6
                    elide: Text.ElideRight
                }

                // Source badge
                Rectangle {
                    visible: root.audioSource !== "none" && root.isPlaying
                    width: sourceBadgeText.width + 12
                    height: 16
                    radius: 3
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                    border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                    border.width: 1

                    Text {
                        id: sourceBadgeText
                        anchors.centerIn: parent
                        text: root.sourceName
                        color: ThemeValues.primaryCol
                        font.pixelSize: 10
                        font.family: ThemeValues.fontFamily
                        font.weight: Font.Bold
                    }
                }
            }

            // Controls
            Row {
                id: controlsMini
                anchors.verticalCenter: parent.verticalCenter
                spacing: root._ctrlSpacing

                // Play/Pause
                Rectangle {
                    width: root._ctrlSize
                    height: root._ctrlSize
                    radius: width / 2
                    color: "transparent"
                    border.color: ThemeValues.primaryCol
                    border.width: root._borderW

                    Image {
                        anchors.centerIn: parent
                        source: root.isPlaying ? iconSource("pause") : iconSource("play")
                        width: root._iconSize
                        height: root._iconSize
                        fillMode: Image.PreserveAspectFit

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: ThemeValues.primaryCol
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.isPlaying ? root.doPause() : root.doPlay()
                    }
                }

                // Next
                Rectangle {
                    width: root._ctrlSize
                    height: root._ctrlSize
                    radius: width / 2
                    color: "transparent"
                    border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.5)
                    border.width: root._borderW

                    Image {
                        anchors.centerIn: parent
                        source: iconSource("next")
                        width: root._iconSize
                        height: root._iconSize
                        fillMode: Image.PreserveAspectFit

                        layer.enabled: true
                        layer.effect: ColorOverlay {
                            color: ThemeValues.primaryCol
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.doNext()
                    }
                }
            }
        }
    }

    // ── Expanded view ──
    Item {
        id: expandedView
        anchors.fill: parent
        visible: root.expanded

        Column {
            anchors.fill: parent
            anchors.margins: ThemeValues.pageMargin
            spacing: ThemeValues.gap

            // Header with source name + close
            Row {
                width: parent.width
                height: 40

                Text {
                    text: root.sourceName
                    color: ThemeValues.primaryCol
                    font.family: ThemeValues.fontFamily
                    font.pixelSize: ThemeValues.fontSize + 4
                    font.weight: Font.Bold
                }

                Item { width: parent.width - 200; height: 1 }

                Rectangle {
                    width: 40; height: 40; radius: 20
                    color: "transparent"
                    border.color: ThemeValues.primaryCol
                    border.width: root._borderW

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: ThemeValues.primaryCol
                        font.pixelSize: 24
                        font.family: ThemeValues.fontFamily
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.expanded = false
                    }
                }
            }

            // Album art (large)
            Rectangle {
                width: 200; height: 200
                anchors.horizontalCenter: parent.horizontalCenter
                radius: root._artRadius * 1.5
                color: ThemeValues.cardBgCol
                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                border.width: root._borderW

                Image {
                    id: expandedArt
                    anchors.fill: parent
                    anchors.margins: 2
                    source: root.albumArtUrl
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                    smooth: true
                }

                // Fallback icon
                Image {
                    anchors.centerIn: parent
                    source: iconSource("music")
                    width: 64; height: 64
                    fillMode: Image.PreserveAspectFit
                    opacity: 0.3
                    visible: expandedArt.status !== Image.Ready

                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: ThemeValues.primaryCol
                    }
                }
            }

            // Track info
            Column {
                width: parent.width
                spacing: 6

                Text {
                    width: parent.width
                    text: root.currentTrack || "No Track"
                    color: ThemeValues.textCol
                    font.family: ThemeValues.fontFamily
                    font.pixelSize: ThemeValues.fontSize + 2
                    font.weight: Font.Bold
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: root.currentArtist
                    color: ThemeValues.textCol
                    font.family: ThemeValues.fontFamily
                    font.pixelSize: ThemeValues.fontSize - 2
                    opacity: 0.7
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    width: parent.width
                    text: root.currentAlbum
                    color: ThemeValues.textCol
                    font.family: ThemeValues.fontFamily
                    font.pixelSize: ThemeValues.fontSize - 4
                    opacity: 0.5
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Item { height: 8 }

            // Playback controls
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: root._ctrlSpacing * 1.5

                // Previous
                Rectangle {
                    width: root._ctrlSizeExp; height: root._ctrlSizeExp
                    radius: width / 2
                    color: "transparent"
                    border.color: ThemeValues.primaryCol
                    border.width: root._borderW

                    Image {
                        anchors.centerIn: parent
                        source: iconSource("previous")
                        width: root._iconSizeExp; height: root._iconSizeExp
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.effect: ColorOverlay { color: ThemeValues.primaryCol }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.doPrevious()
                    }
                }

                // Play/Pause (filled)
                Rectangle {
                    width: root._playSize; height: root._playSize
                    radius: width / 2
                    color: ThemeValues.primaryCol

                    Image {
                        anchors.centerIn: parent
                        source: root.isPlaying ? iconSource("pause") : iconSource("play")
                        width: root._playIconSize; height: root._playIconSize
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.effect: ColorOverlay { color: ThemeValues.bgCol }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.isPlaying ? root.doPause() : root.doPlay()
                    }
                }

                // Next
                Rectangle {
                    width: root._ctrlSizeExp; height: root._ctrlSizeExp
                    radius: width / 2
                    color: "transparent"
                    border.color: ThemeValues.primaryCol
                    border.width: root._borderW

                    Image {
                        anchors.centerIn: parent
                        source: iconSource("next")
                        width: root._iconSizeExp; height: root._iconSizeExp
                        fillMode: Image.PreserveAspectFit
                        layer.enabled: true
                        layer.effect: ColorOverlay { color: ThemeValues.primaryCol }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.doNext()
                    }
                }
            }
        }

        // Swipe to collapse
        MouseArea {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 50
            property real startY: 0
            onPressed: startY = mouse.y
            onReleased: {
                if (mouse.y - startY > 30) root.expanded = false
            }
        }
    }

    // Top border accent line
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: root._borderW
        color: ThemeValues.primaryCol
        opacity: 0.5

        layer.enabled: root._useGlow
        layer.effect: Glow {
            samples: 15
            color: ThemeValues.primaryCol
            spread: 0.5
        }
    }
}
