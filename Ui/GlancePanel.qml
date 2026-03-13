import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

Item {
    id: root

    property var theme: null

    // Navigation state (bound from ScreenContainer via Main.qml)
    property bool navActive: false
    property string maneuver: ""
    property string instruction: ""
    property string stepDistance: ""
    property string routeDuration: ""

    // Audio state
    property string audioSource: "none"

    // Visibility control
    property bool isHomeScreen: false

    // Signals
    signal navTapped()
    signal musicTapped()

    // ── Multi-source audio bindings (driven by audioSource from Main.qml) ──
    readonly property bool isTidal: audioSource === "tidal"
    readonly property bool isSpotify: audioSource === "spotify"
    readonly property bool isBluetooth: audioSource === "music"

    readonly property string currentTrack:
        isTidal ? (tidalClient ? tidalClient.trackTitle : "")
        : isSpotify ? (spotifyClient ? spotifyClient.trackTitle : "")
        : isBluetooth ? (mediaController ? mediaController.trackTitle : "")
        : ""

    readonly property string currentArtist:
        isTidal ? (tidalClient ? tidalClient.artist : "")
        : isSpotify ? (spotifyClient ? spotifyClient.artist : "")
        : isBluetooth ? (mediaController ? mediaController.artist : "")
        : ""

    readonly property bool isPlaying:
        isTidal ? (tidalClient ? tidalClient.isPlaying : false)
        : isSpotify ? (spotifyClient ? spotifyClient.isPlaying : false)
        : isBluetooth ? (mediaController ? mediaController.isPlaying : false)
        : false

    readonly property real trackPosition:
        isTidal ? (tidalClient ? tidalClient.position : 0)
        : isSpotify ? (spotifyClient ? spotifyClient.position : 0)
        : isBluetooth ? (mediaController ? mediaController.trackPosition : 0)
        : 0

    readonly property real trackDuration:
        isTidal ? (tidalClient ? tidalClient.duration : 0)
        : isSpotify ? (spotifyClient ? spotifyClient.duration : 0)
        : isBluetooth ? (mediaController ? mediaController.trackDuration : 0)
        : 0

    readonly property real trackProgress:
        trackDuration > 0 ? trackPosition / trackDuration : 0

    function doPlay()     { isTidal ? tidalClient.resume() : isSpotify ? spotifyClient.resume() : mediaController.play() }
    function doPause()    { isTidal ? tidalClient.pause()  : isSpotify ? spotifyClient.pause()  : mediaController.pause() }
    function doNext()     { isTidal ? tidalClient.next()   : isSpotify ? spotifyClient.next()   : mediaController.next() }
    function doPrevious() { isTidal ? tidalClient.previous() : isSpotify ? spotifyClient.previous() : mediaController.previous() }

    // ── Call state ──
    readonly property bool hasActiveCall: bluetoothManager ? bluetoothManager.hasActiveCall : false

    // ── Content visibility ──
    readonly property bool hasTrack: audioSource !== "none"
    readonly property bool musicVisible: !hasActiveCall && hasTrack
    readonly property bool hasContent: navActive || musicVisible || hasActiveCall

    onHasContentChanged: console.log("GlancePanel hasContent:", hasContent, "isHome:", isHomeScreen, "audioSrc:", audioSource, "hasTrack:", hasTrack)
    onIsHomeScreenChanged: console.log("GlancePanel isHomeScreen:", isHomeScreen)
    onAudioSourceChanged: console.log("GlancePanel audioSource:", audioSource)
    onCurrentTrackChanged: console.log("GlancePanel track:", currentTrack)

    // ── Slide in/out ──
    x: (isHomeScreen && hasContent) ? (parent.width - width) : parent.width

    Behavior on x {
        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
    }

    clip: true

    // ── Panel background ──
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.80)

        // Left accent border
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 2
            color: ThemeValues.primaryCol
            opacity: 0.6
        }

        Column {
            anchors.fill: parent
            anchors.margins: 16
            anchors.leftMargin: 18  // Account for accent border
            spacing: 16

            // ════════════════════════════════════════
            //  NAVIGATION CARD
            // ════════════════════════════════════════
            Rectangle {
                id: navCard
                width: parent.width
                height: 130
                radius: ThemeValues.radius
                color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.08)
                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                border.width: 1
                visible: root.navActive

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    // Maneuver row: icon + distance
                    Row {
                        width: parent.width
                        spacing: 12

                        // Maneuver icon
                        Rectangle {
                            width: 44; height: 44
                            radius: 10
                            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)

                            Text {
                                anchors.centerIn: parent
                                text: maneuverIcon(root.maneuver)
                                font.pixelSize: 24
                                font.family: ThemeValues.fontFamily
                                color: ThemeValues.primaryCol
                            }
                        }

                        // Distance to next turn
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2

                            Text {
                                text: root.stepDistance || ""
                                color: ThemeValues.primaryCol
                                font.pixelSize: ThemeValues.fontSize + 6
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                            }

                            Text {
                                text: root.maneuver.replace(/-/g, " ").replace(/\b\w/g, function(c) { return c.toUpperCase() })
                                color: ThemeValues.textCol
                                font.pixelSize: ThemeValues.fontSize - 3
                                font.family: ThemeValues.fontFamily
                                opacity: 0.5
                            }
                        }
                    }

                    // Instruction
                    Text {
                        width: parent.width
                        text: root.instruction
                        color: ThemeValues.textCol
                        font.pixelSize: ThemeValues.fontSize - 1
                        font.family: ThemeValues.fontFamily
                        opacity: 0.7
                        maximumLineCount: 2
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                    }

                    // ETA badge
                    Rectangle {
                        width: etaText.width + 16
                        height: 22
                        radius: 4
                        color: Qt.rgba(ThemeValues.accentCol.r, ThemeValues.accentCol.g, ThemeValues.accentCol.b, 0.15)
                        border.color: Qt.rgba(ThemeValues.accentCol.r, ThemeValues.accentCol.g, ThemeValues.accentCol.b, 0.3)
                        border.width: 1

                        Text {
                            id: etaText
                            anchors.centerIn: parent
                            text: "ETA " + computeETA(root.routeDuration)
                            color: ThemeValues.accentCol
                            font.pixelSize: ThemeValues.fontSize - 3
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                        }
                    }
                }

            }

            // ════════════════════════════════════════
            //  NOW PLAYING CARD
            // ════════════════════════════════════════
            Rectangle {
                id: nowPlayingCard
                width: parent.width
                height: root.navActive ? parent.height - navCard.height - parent.spacing - 32 : parent.height - 32
                radius: ThemeValues.radius
                color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.06)
                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                border.width: 1
                visible: root.musicVisible
                clip: true

                // Progress bar at top
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    width: parent.width * root.trackProgress
                    height: 3
                    radius: 1.5
                    color: ThemeValues.primaryCol

                    Behavior on width {
                        NumberAnimation { duration: 250 }
                    }
                }

                // Track background
                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 3
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.1)
                }

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    anchors.topMargin: 18  // Below progress bar
                    spacing: 8

                    // Source badge
                    Rectangle {
                        width: sourceText.width + 12
                        height: 18
                        radius: 3
                        color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.12)

                        Text {
                            id: sourceText
                            anchors.centerIn: parent
                            text: root.isTidal ? "TIDAL" : root.isSpotify ? "SPOTIFY" : "BLUETOOTH"
                            color: ThemeValues.primaryCol
                            font.pixelSize: ThemeValues.fontSize - 4
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                        }
                    }

                    // Track title
                    Text {
                        width: parent.width
                        text: root.currentTrack || "No Track"
                        color: ThemeValues.textCol
                        font.pixelSize: ThemeValues.fontSize
                        font.family: ThemeValues.fontFamily
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }

                    // Artist
                    Text {
                        width: parent.width
                        text: root.currentArtist
                        color: ThemeValues.textCol
                        font.pixelSize: ThemeValues.fontSize - 2
                        font.family: ThemeValues.fontFamily
                        opacity: 0.6
                        elide: Text.ElideRight
                    }

                    Item { width: 1; height: 8 }

                    // Controls
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 24

                        // Previous
                        Rectangle {
                            width: 48; height: 48
                            radius: 24
                            color: "transparent"
                            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                            border.width: 1

                            Image {
                                anchors.centerIn: parent
                                source: (theme && theme.iconPath) ? theme.iconPath("previous") : ""
                                width: 20; height: 20
                                fillMode: Image.PreserveAspectFit
                                layer.enabled: true
                                layer.effect: ColorOverlay { color: ThemeValues.primaryCol }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: root.doPrevious()
                            }
                        }

                        // Play/Pause
                        Rectangle {
                            width: 56; height: 56
                            radius: 28
                            color: ThemeValues.primaryCol

                            Image {
                                anchors.centerIn: parent
                                source: (theme && theme.iconPath) ? theme.iconPath(root.isPlaying ? "pause" : "play") : ""
                                width: 24; height: 24
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
                            width: 48; height: 48
                            radius: 24
                            color: "transparent"
                            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                            border.width: 1

                            Image {
                                anchors.centerIn: parent
                                source: (theme && theme.iconPath) ? theme.iconPath("next") : ""
                                width: 20; height: 20
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

            }

            // ════════════════════════════════════════
            //  CALL CARD (replaces now playing)
            // ════════════════════════════════════════
            Rectangle {
                id: callCard
                width: parent.width
                height: root.navActive ? parent.height - navCard.height - parent.spacing - 32 : parent.height - 32
                radius: ThemeValues.radius
                color: Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.06)
                border.color: Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.2)
                border.width: 1
                visible: root.hasActiveCall

                Column {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    // "In Call" badge
                    Rectangle {
                        width: callBadgeText.width + 12
                        height: 18
                        radius: 3
                        color: Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.15)

                        Text {
                            id: callBadgeText
                            anchors.centerIn: parent
                            text: "IN CALL"
                            color: ThemeValues.errorCol
                            font.pixelSize: ThemeValues.fontSize - 4
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                        }
                    }

                    // Caller name
                    Text {
                        width: parent.width
                        text: getCallerDisplay()
                        color: ThemeValues.textCol
                        font.pixelSize: ThemeValues.fontSize + 2
                        font.family: ThemeValues.fontFamily
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }

                    // Call duration
                    Text {
                        text: formatDuration(bluetoothManager ? bluetoothManager.activeCallDuration : 0)
                        color: ThemeValues.textCol
                        font.pixelSize: ThemeValues.fontSize
                        font.family: ThemeValues.fontFamily
                        opacity: 0.5
                    }

                    Item { width: 1; height: 8 }

                    // Call controls
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 20

                        // Mute toggle
                        Rectangle {
                            width: 56; height: 56
                            radius: 28
                            color: (bluetoothManager && bluetoothManager.isCallMuted)
                                ? Qt.rgba(ThemeValues.warningCol.r, ThemeValues.warningCol.g, ThemeValues.warningCol.b, 0.2)
                                : "transparent"
                            border.color: (bluetoothManager && bluetoothManager.isCallMuted)
                                ? ThemeValues.warningCol
                                : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                            border.width: 1

                            // Mute icon (Canvas)
                            Canvas {
                                id: muteCanvas
                                anchors.centerIn: parent
                                width: 24; height: 24
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    var muted = bluetoothManager && bluetoothManager.isCallMuted
                                    ctx.strokeStyle = muted ? ThemeValues.warningCol.toString() : ThemeValues.textCol.toString()
                                    ctx.fillStyle = ctx.strokeStyle
                                    ctx.lineWidth = 2
                                    ctx.lineCap = "round"
                                    // Microphone body
                                    ctx.beginPath()
                                    ctx.arc(12, 8, 4, Math.PI, 0)
                                    ctx.lineTo(16, 14)
                                    ctx.arc(12, 14, 4, 0, Math.PI)
                                    ctx.closePath()
                                    ctx.fill()
                                    // Stand
                                    ctx.beginPath()
                                    ctx.arc(12, 14, 6, 0, Math.PI)
                                    ctx.stroke()
                                    ctx.beginPath()
                                    ctx.moveTo(12, 20)
                                    ctx.lineTo(12, 23)
                                    ctx.stroke()
                                    // Strike-through if muted
                                    if (muted) {
                                        ctx.beginPath()
                                        ctx.moveTo(4, 2)
                                        ctx.lineTo(20, 22)
                                        ctx.stroke()
                                    }
                                }
                            }

                            // Force repaint when mute changes
                            Connections {
                                target: bluetoothManager
                                function onCallMutedChanged() {
                                    muteCanvas.requestPaint()
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (bluetoothManager) bluetoothManager.toggleMute()
                                }
                            }
                        }

                        // End call
                        Rectangle {
                            width: 80; height: 56
                            radius: 28
                            color: ThemeValues.errorCol

                            // Hang up icon (Canvas)
                            Canvas {
                                anchors.centerIn: parent
                                width: 28; height: 28
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.strokeStyle = ThemeValues.bgCol.toString()
                                    ctx.fillStyle = ThemeValues.bgCol.toString()
                                    ctx.lineWidth = 2.5
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"
                                    // Rotated handset (hang up)
                                    ctx.save()
                                    ctx.translate(14, 14)
                                    ctx.rotate(Math.PI * 0.75)
                                    // Earpiece
                                    ctx.beginPath()
                                    ctx.arc(-6, -4, 3, 0, Math.PI * 2)
                                    ctx.fill()
                                    // Mouthpiece
                                    ctx.beginPath()
                                    ctx.arc(-6, 8, 3, 0, Math.PI * 2)
                                    ctx.fill()
                                    // Handle
                                    ctx.beginPath()
                                    ctx.moveTo(-6, -1)
                                    ctx.lineTo(-6, 5)
                                    ctx.stroke()
                                    ctx.restore()
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (bluetoothManager) bluetoothManager.hangupCall()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Helper functions ──

    function maneuverIcon(maneuver) {
        if (maneuver.indexOf("arrive") >= 0)        return "\u2691"
        if (maneuver.indexOf("depart") >= 0)         return "\u2690"
        if (maneuver.indexOf("straight") >= 0)       return "\u2191"
        if (maneuver.indexOf("slight-right") >= 0)   return "\u2197"
        if (maneuver.indexOf("right") >= 0)          return "\u2192"
        if (maneuver.indexOf("sharp-right") >= 0)    return "\u21B1"
        if (maneuver.indexOf("slight-left") >= 0)    return "\u2196"
        if (maneuver.indexOf("left") >= 0)           return "\u2190"
        if (maneuver.indexOf("sharp-left") >= 0)     return "\u21B0"
        if (maneuver.indexOf("uturn") >= 0)          return "\u21BA"
        if (maneuver.indexOf("merge") >= 0)          return "\u2934"
        if (maneuver.indexOf("roundabout") >= 0)     return "\u21BB"
        if (maneuver.indexOf("rotary") >= 0)         return "\u21BB"
        if (maneuver.indexOf("fork") >= 0)           return "\u2195"
        if (maneuver.indexOf("ramp") >= 0)           return "\u2197"
        return "\u2191"
    }

    function computeETA(durationStr) {
        if (!durationStr) return "--:--"
        var mins = 0
        var hrMatch = durationStr.match(/(\d+)\s*hr/)
        var minMatch = durationStr.match(/(\d+)\s*min/)
        if (hrMatch) mins += parseInt(hrMatch[1]) * 60
        if (minMatch) mins += parseInt(minMatch[1])
        if (mins === 0) return "--:--"
        var now = new Date()
        now.setMinutes(now.getMinutes() + mins)
        var h = now.getHours()
        var m = now.getMinutes()
        return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m
    }

    function getCallerDisplay() {
        if (!bluetoothManager) return "Unknown"
        var name = bluetoothManager.activeCallName
        var number = bluetoothManager.activeCallNumber
        if (name && name.length > 0) return name
        if (number && number.length > 0) return number
        return "Unknown Caller"
    }

    function formatDuration(seconds) {
        var mins = Math.floor(seconds / 60)
        var secs = seconds % 60
        return (mins < 10 ? "0" : "") + mins + ":" + (secs < 10 ? "0" : "") + secs
    }
}
