import QtQuick 2.15
import Qt5Compat.GraphicalEffects

/**
 * ActiveCallOverlay - Premium In-Call UI
 *
 * Displays active call information with refined, high-class aesthetics
 * Appears when a phone call is active via Bluetooth HFP
 */
Rectangle {
    id: root
    anchors.fill: parent
    color: "#000000"

    // -------- API --------
    property var bluetoothManager: null
    property var theme: null  // Optional theme support

    // -------- Visibility Control --------
    visible: bluetoothManager ? bluetoothManager.hasActiveCall : false
    opacity: visible ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 400; easing.type: Easing.InOutCubic }
    }

    // -------- Premium Gradient Background --------
    gradient: Gradient {
        GradientStop { position: 0.0; color: "#0D0D0F" }
        GradientStop { position: 0.3; color: "#121216" }
        GradientStop { position: 0.7; color: "#0F0F12" }
        GradientStop { position: 1.0; color: "#0A0A0C" }
    }

    // -------- Subtle Radial Accent --------
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 1.2
        height: parent.height * 1.2
        radius: width / 2
        opacity: 0.03
        gradient: Gradient {
            GradientStop { position: 0.0; color: getStateColor() }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // -------- Fine Grid Overlay (Subtle) --------
    Canvas {
        anchors.fill: parent
        opacity: 0.02

        onPaint: {
            var ctx = getContext("2d")
            ctx.strokeStyle = "#FFFFFF"
            ctx.lineWidth = 0.5

            var spacing = 60
            for (var x = 0; x < width; x += spacing) {
                ctx.beginPath()
                ctx.moveTo(x, 0)
                ctx.lineTo(x, height)
                ctx.stroke()
            }
            for (var y = 0; y < height; y += spacing) {
                ctx.beginPath()
                ctx.moveTo(0, y)
                ctx.lineTo(width, y)
                ctx.stroke()
            }
        }
    }

    // -------- Main Content Container --------
    Item {
        anchors.fill: parent
        anchors.topMargin: 80
        anchors.bottomMargin: 80

        Column {
            anchors.centerIn: parent
            spacing: 50
            width: parent.width * 0.75

            // -------- Call State Badge --------
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: statusLabel.width + 48
                height: 46
                radius: 23
                color: Qt.rgba(0, 0, 0, 0.4)
                border.width: 1.5
                border.color: Qt.rgba(1, 1, 1, 0.12)

                // Inner state indicator
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: getStateColor()

                    layer.enabled: true
                    layer.effect: Glow {
                        color: getStateColor()
                        spread: 0.6
                        radius: 8
                        samples: 17
                    }

                    // Gentle pulse for active calls
                    SequentialAnimation on opacity {
                        running: root.visible && bluetoothManager && bluetoothManager.activeCallState === "active"
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 1200; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1200; easing.type: Easing.InOutSine }
                    }
                }

                Text {
                    id: statusLabel
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 6
                    text: getStateText()
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    font.letterSpacing: 1.2
                    font.family: "SF Pro Display"
                    color: "#FFFFFF"
                    opacity: 0.85
                }
            }

            // -------- Caller Information --------
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 18

                // Caller Name/Number
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: getCallerDisplay()
                    font.pixelSize: 44
                    font.weight: Font.Light
                    font.letterSpacing: -0.5
                    font.family: "SF Pro Display"
                    color: "#FFFFFF"

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: Qt.rgba(0, 0, 0, 0.4)
                        radius: 12
                        samples: 25
                        horizontalOffset: 0
                        verticalOffset: 4
                    }
                }

                // Call Duration Timer
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: formatDuration(bluetoothManager ? bluetoothManager.activeCallDuration : 0)
                    font.pixelSize: 28
                    font.weight: Font.Normal
                    font.letterSpacing: 1.5
                    font.family: "SF Mono"
                    color: "#FFFFFF"
                    opacity: 0.5
                }
            }

            // -------- Spacer --------
            Item { width: 1; height: 30 }

            // -------- Call Action Buttons --------
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 50

                // Mute Button (Secondary)
                Rectangle {
                    id: muteButton
                    width: 140
                    height: 140
                    radius: 70
                    opacity: (bluetoothManager && bluetoothManager.isCallMuted) ? 1.0 : 0.7

                    // Gradient background - changes when muted
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: (bluetoothManager && bluetoothManager.isCallMuted) ? "#E63946" : "#2A2A2E"
                        }
                        GradientStop {
                            position: 1.0
                            color: (bluetoothManager && bluetoothManager.isCallMuted) ? "#B8242D" : "#1C1C1F"
                        }
                    }

                    // Subtle outer ring
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width + 4
                        height: parent.height + 4
                        radius: width / 2
                        color: "transparent"
                        border.width: 1.5
                        border.color: (bluetoothManager && bluetoothManager.isCallMuted) ? Qt.rgba(230, 57, 70, 0.3) : Qt.rgba(1, 1, 1, 0.08)
                    }

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: (bluetoothManager && bluetoothManager.isCallMuted) ? Qt.rgba(230, 57, 70, 0.4) : Qt.rgba(0, 0, 0, 0.3)
                        radius: (bluetoothManager && bluetoothManager.isCallMuted) ? 16 : 12
                        samples: 25
                        horizontalOffset: 0
                        verticalOffset: 4
                    }

                    // Mute Icon - changes based on mute state
                    Text {
                        anchors.centerIn: parent
                        text: (bluetoothManager && bluetoothManager.isCallMuted) ? "🔇" : "🔊"
                        font.pixelSize: 48
                        opacity: 0.8
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            console.log("ActiveCallOverlay: Mute button pressed")
                            if (bluetoothManager) {
                                bluetoothManager.toggleMute()
                            }
                        }

                        onPressed: parent.scale = 0.94
                        onReleased: parent.scale = 1.0
                        onCanceled: parent.scale = 1.0
                    }

                    Behavior on scale {
                        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }

                    Behavior on opacity {
                        NumberAnimation { duration: 200 }
                    }
                }

                // End Call Button (Primary)
                Rectangle {
                    id: endCallButton
                    width: 180
                    height: 180
                    radius: 90

                // Gradient background
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#E63946" }
                    GradientStop { position: 1.0; color: "#B8242D" }
                }

                // Subtle outer ring
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width + 4
                    height: parent.height + 4
                    radius: width / 2
                    color: "transparent"
                    border.width: 1.5
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                }

                layer.enabled: true
                layer.effect: DropShadow {
                    color: Qt.rgba(230, 57, 70, 0.4)
                    radius: 20
                    samples: 41
                    horizontalOffset: 0
                    verticalOffset: 8
                }

                // Phone Icon
                Item {
                    anchors.centerIn: parent
                    width: 56
                    height: 56

                    // Handset shape using path
                    Canvas {
                        anchors.fill: parent
                        rotation: 135

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.fillStyle = "#FFFFFF"
                            ctx.beginPath()
                            ctx.roundedRect(12, 4, 32, 12, 6, 6)
                            ctx.fill()
                            ctx.beginPath()
                            ctx.roundedRect(12, 40, 32, 12, 6, 6)
                            ctx.fill()
                            ctx.beginPath()
                            ctx.arc(28, 28, 4, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        console.log("ActiveCallOverlay: End call button pressed")
                        if (bluetoothManager) {
                            bluetoothManager.hangupCall()
                        }
                    }

                    onPressed: parent.scale = 0.94
                    onReleased: parent.scale = 1.0
                    onCanceled: parent.scale = 1.0
                }

                Behavior on scale {
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }
            }
            }  // End Row

            // -------- Spacer --------
            Item { width: 1; height: 20 }

            // -------- Device Connection Info --------
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                opacity: 0.4

                // Bluetooth Icon
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u{f293}"  // Bluetooth symbol
                    font.family: "Font Awesome 6 Free"
                    font.pixelSize: 14
                    color: "#FFFFFF"
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: getConnectedDeviceName()
                    font.pixelSize: 15
                    font.weight: Font.Normal
                    font.letterSpacing: 0.3
                    font.family: "SF Pro Display"
                    color: "#FFFFFF"
                }
            }
        }
    }

    // -------- Decorative Top Bar --------
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        opacity: 0.08
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: "#FFFFFF" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // -------- Decorative Bottom Bar --------
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        opacity: 0.08
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 0.5; color: "#FFFFFF" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // -------- Helper Functions --------

    function getStateText() {
        if (!bluetoothManager) return "Unknown"

        var state = bluetoothManager.activeCallState
        switch(state) {
            case "dialing": return "Calling"
            case "alerting": return "Ringing"
            case "active": return "In Call"
            case "held": return "On Hold"
            case "incoming": return "Incoming Call"
            default: return state
        }
    }

    function getStateColor() {
        if (!bluetoothManager) return "#4A9EFF"

        var state = bluetoothManager.activeCallState
        switch(state) {
            case "dialing": return "#FFA726"   // Warm amber
            case "alerting": return "#FFA726"   // Warm amber
            case "active": return "#66BB6A"     // Soft green
            case "held": return "#EF5350"       // Soft red
            case "incoming": return "#42A5F5"   // Calm blue
            default: return "#4A9EFF"           // Default blue
        }
    }

    function getCallerDisplay() {
        if (!bluetoothManager) return "Unknown"

        var name = bluetoothManager.activeCallName
        var number = bluetoothManager.activeCallNumber

        // Show name if available, otherwise show number
        if (name && name.length > 0) {
            return name
        } else if (number && number.length > 0) {
            return number
        } else {
            return "Unknown Caller"
        }
    }

    function formatDuration(seconds) {
        var mins = Math.floor(seconds / 60)
        var secs = seconds % 60
        return (mins < 10 ? "0" : "") + mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    function getConnectedDeviceName() {
        if (!bluetoothManager) return "Bluetooth Device"

        var address = bluetoothManager.getFirstConnectedDeviceAddress()
        if (address) {
            var name = bluetoothManager.getDeviceName(address)
            return name ? name : "Bluetooth Device"
        }
        return "Bluetooth Device"
    }

    // -------- Debug Info --------
    Component.onCompleted: {
        console.log("ActiveCallOverlay: Premium UI loaded")
    }

    onVisibleChanged: {
        console.log("ActiveCallOverlay: Visibility changed to:", visible)
        if (visible && bluetoothManager) {
            console.log("  - Call State:", bluetoothManager.activeCallState)
            console.log("  - Caller:", getCallerDisplay())
        }
    }
}
