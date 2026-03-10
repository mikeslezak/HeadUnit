import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

/**
 * CallOverlayBase - Shared base for call overlay screens
 *
 * Provides the premium background, status badge, caller info,
 * and helper functions used by both ActiveCallOverlay and IncomingCallOverlay.
 */
Rectangle {
    id: root
    anchors.fill: parent
    color: ThemeValues.bgCol

    // -------- API --------
    property var bluetoothManager: null
    property var theme: null

    // Customization points for subclasses
    property string badgeText: "Call"
    property color accentColor: ThemeValues.primaryCol
    property bool badgePulse: true
    property alias contentItem: contentArea

    // -------- Premium Gradient Background --------
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.darker(ThemeValues.bgCol, 1.1) }
        GradientStop { position: 0.3; color: ThemeValues.bgCol }
        GradientStop { position: 0.7; color: Qt.darker(ThemeValues.bgCol, 1.05) }
        GradientStop { position: 1.0; color: Qt.darker(ThemeValues.bgCol, 1.2) }
    }

    Behavior on opacity {
        NumberAnimation { duration: 400; easing.type: Easing.InOutCubic }
    }

    // -------- Subtle Radial Accent --------
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 1.2
        height: parent.height * 1.2
        radius: width / 2
        opacity: 0.03
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.accentColor }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // -------- Fine Grid Overlay --------
    Canvas {
        anchors.fill: parent
        opacity: 0.02

        onPaint: {
            var ctx = getContext("2d")
            ctx.strokeStyle = ThemeValues.textCol
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

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 4
                    color: root.accentColor

                    layer.enabled: true
                    layer.effect: Glow {
                        color: root.accentColor
                        spread: 0.6
                        radius: 8
                        samples: 17
                    }

                    SequentialAnimation on opacity {
                        running: root.visible && root.badgePulse
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.3; duration: 1200; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1200; easing.type: Easing.InOutSine }
                    }
                }

                Text {
                    id: statusLabel
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: 6
                    text: root.badgeText
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    font.letterSpacing: 1.2
                    font.family: "SF Pro Display"
                    color: ThemeValues.textCol
                    opacity: 0.85
                }
            }

            // -------- Caller Information --------
            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 18

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: getCallerDisplay()
                    font.pixelSize: 44
                    font.weight: Font.Light
                    font.letterSpacing: -0.5
                    font.family: "SF Pro Display"
                    color: ThemeValues.textCol

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: Qt.rgba(0, 0, 0, 0.4)
                        radius: 12
                        samples: 25
                        horizontalOffset: 0
                        verticalOffset: 4
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: formatDuration(bluetoothManager ? bluetoothManager.activeCallDuration : 0)
                    font.pixelSize: 28
                    font.weight: Font.Normal
                    font.letterSpacing: 1.5
                    font.family: "SF Mono"
                    color: ThemeValues.textCol
                    opacity: 0.5
                }
            }

            // -------- Spacer --------
            Item { width: 1; height: 30 }

            // -------- Slot for subclass-specific content (buttons, etc.) --------
            Item {
                id: contentArea
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                implicitHeight: childrenRect.height
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
            GradientStop { position: 0.5; color: ThemeValues.textCol }
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
            GradientStop { position: 0.5; color: ThemeValues.textCol }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // -------- Helper Functions --------

    function getCallerDisplay() {
        if (!bluetoothManager) return "Unknown"

        var name = bluetoothManager.activeCallName
        var number = bluetoothManager.activeCallNumber

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
}
