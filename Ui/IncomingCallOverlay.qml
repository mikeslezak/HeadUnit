import QtQuick 2.15
import Qt5Compat.GraphicalEffects

/**
 * IncomingCallOverlay - Premium Incoming Call UI
 *
 * Displays incoming call information with accept/reject buttons
 * Appears when receiving an incoming phone call via Bluetooth HFP
 */
Rectangle {
    id: root
    anchors.fill: parent
    color: "#000000"

    // -------- API --------
    property var bluetoothManager: null
    property var theme: null  // Optional theme support

    // -------- Visibility Control --------
    visible: bluetoothManager ? (bluetoothManager.hasActiveCall && bluetoothManager.activeCallState === "incoming") : false
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

    // -------- Subtle Radial Accent (Blue for incoming) --------
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 1.2
        height: parent.height * 1.2
        radius: width / 2
        opacity: 0.03
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#42A5F5" }
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

            // -------- Incoming Call Badge --------
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: statusLabel.width + 48
                height: 46
                radius: 23
                color: Qt.rgba(0, 0, 0, 0.4)
                border.width: 1.5
                border.color: Qt.rgba(1, 1, 1, 0.12)

                // Inner state indicator (pulsing blue)
                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: "#42A5F5"

                    layer.enabled: true
                    layer.effect: Glow {
                        color: "#42A5F5"
                        spread: 0.6
                        radius: 8
                        samples: 17
                    }

                    // Gentle pulse for incoming calls
                    SequentialAnimation on opacity {
                        running: root.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 1200; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1200; easing.type: Easing.InOutSine }
                    }
                }

                Text {
                    id: statusLabel
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 6
                    text: "Incoming Call"
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    font.letterSpacing: 1.2
                    font.family: "SF Pro Display"
                    color: "#FFFFFF"
                    opacity: 0.85
                }
            }

            // -------- Phone Icon Animation --------
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 120
                height: 120

                // Pulsing ring effect
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: Qt.rgba(66, 165, 245, 0.3)

                    SequentialAnimation on scale {
                        running: root.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.3; duration: 1500; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1500; easing.type: Easing.InOutSine }
                    }

                    SequentialAnimation on opacity {
                        running: root.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.0; duration: 1500; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 0.3; duration: 1500; easing.type: Easing.InOutSine }
                    }
                }

                // Phone icon
                Text {
                    anchors.centerIn: parent
                    text: "📞"
                    font.pixelSize: 64
                    opacity: 0.9
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

                // Call Duration (Ring Time)
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: formatDuration(bluetoothManager ? bluetoothManager.activeCallDuration : 0)
                    font.pixelSize: 28
                    font.weight: Font.Medium
                    font.letterSpacing: 0.8
                    font.family: "SF Pro Display"
                    color: "#FFFFFF"
                    opacity: 0.7
                }

                // Secondary info
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: getDeviceName()
                    font.pixelSize: 18
                    font.weight: Font.Normal
                    font.letterSpacing: 0.5
                    font.family: "SF Pro Display"
                    color: "#FFFFFF"
                    opacity: 0.5
                }
            }

            // -------- Spacer --------
            Item { width: 1; height: 30 }

            // -------- Call Action Buttons --------
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 80

                // Reject Button (Red)
                Rectangle {
                    id: rejectButton
                    width: 160
                    height: 160
                    radius: 80

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
                        border.color: Qt.rgba(230, 57, 70, 0.3)
                    }

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: Qt.rgba(230, 57, 70, 0.4)
                        radius: 20
                        samples: 41
                        horizontalOffset: 0
                        verticalOffset: 8
                    }

                    // Decline Icon (X)
                    Text {
                        anchors.centerIn: parent
                        text: "✖"
                        font.pixelSize: 56
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                        opacity: 0.9
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            console.log("IncomingCallOverlay: Reject button pressed")
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

                // Accept Button (Green)
                Rectangle {
                    id: acceptButton
                    width: 200
                    height: 200
                    radius: 100

                    // Gradient background
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#2ECC71" }
                        GradientStop { position: 1.0; color: "#27AE60" }
                    }

                    // Subtle outer ring
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width + 4
                        height: parent.height + 4
                        radius: width / 2
                        color: "transparent"
                        border.width: 1.5
                        border.color: Qt.rgba(46, 204, 113, 0.3)
                    }

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: Qt.rgba(46, 204, 113, 0.4)
                        radius: 24
                        samples: 49
                        horizontalOffset: 0
                        verticalOffset: 10
                    }

                    // Phone Icon
                    Item {
                        anchors.centerIn: parent
                        width: 64
                        height: 64

                        // Handset shape using path
                        Canvas {
                            anchors.fill: parent
                            rotation: 0

                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.fillStyle = "#FFFFFF"
                                ctx.beginPath()
                                ctx.roundedRect(12, 4, 40, 16, 8, 8)
                                ctx.fill()
                                ctx.beginPath()
                                ctx.roundedRect(12, 44, 40, 16, 8, 8)
                                ctx.fill()
                                ctx.beginPath()
                                ctx.arc(32, 32, 5, 0, Math.PI * 2)
                                ctx.fill()
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            console.log("IncomingCallOverlay: Accept button pressed")
                            if (bluetoothManager) {
                                bluetoothManager.answerCall()
                            }
                        }

                        onPressed: parent.scale = 0.94
                        onReleased: parent.scale = 1.0
                        onCanceled: parent.scale = 1.0
                    }

                    Behavior on scale {
                        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }

                    // Gentle pulse effect
                    SequentialAnimation on scale {
                        running: root.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.05; duration: 1000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
                    }
                }
            }  // End Row

            // -------- Action Labels --------
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 80

                // Reject label
                Text {
                    width: 160
                    horizontalAlignment: Text.AlignHCenter
                    text: "Decline"
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    font.letterSpacing: 0.5
                    font.family: "SF Pro Display"
                    color: "#FFFFFF"
                    opacity: 0.6
                }

                // Accept label
                Text {
                    width: 200
                    horizontalAlignment: Text.AlignHCenter
                    text: "Answer"
                    font.pixelSize: 18
                    font.weight: Font.Medium
                    font.letterSpacing: 0.5
                    font.family: "SF Pro Display"
                    color: "#FFFFFF"
                    opacity: 0.7
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

    function getDeviceName() {
        if (!bluetoothManager) return ""

        var address = bluetoothManager.getFirstConnectedDeviceAddress()
        if (address) {
            var name = bluetoothManager.getDeviceName(address)
            return name ? "via " + name : "via Bluetooth"
        }
        return "via Bluetooth"
    }

    function formatDuration(seconds) {
        var mins = Math.floor(seconds / 60)
        var secs = seconds % 60
        return (mins < 10 ? "0" : "") + mins + ":" + (secs < 10 ? "0" : "") + secs
    }

    // -------- Debug Info --------
    Component.onCompleted: {
        console.log("IncomingCallOverlay: Premium UI loaded")
    }

    onVisibleChanged: {
        console.log("IncomingCallOverlay: Visibility changed to:", visible)
        if (visible && bluetoothManager) {
            console.log("  - Caller:", getCallerDisplay())
            console.log("  - Device:", getDeviceName())
        }
    }
}
