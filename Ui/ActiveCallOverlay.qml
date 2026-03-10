import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

/**
 * ActiveCallOverlay - Premium In-Call UI
 *
 * Displays active call information with mute and end-call buttons.
 * Appears when a phone call is active via Bluetooth HFP.
 */
CallOverlayBase {
    id: root

    // -------- Visibility Control --------
    visible: bluetoothManager ? bluetoothManager.hasActiveCall : false
    opacity: visible ? 1.0 : 0.0

    // -------- Dynamic state --------
    badgeText: getStateText()
    accentColor: getStateColor()
    badgePulse: bluetoothManager ? bluetoothManager.activeCallState === "active" : false

    // -------- Call Action Buttons --------
    contentItem.children: [
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 20

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

                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: (bluetoothManager && bluetoothManager.isCallMuted) ? ThemeValues.errorCol : Qt.darker(ThemeValues.cardBgCol, 1.2)
                        }
                        GradientStop {
                            position: 1.0
                            color: (bluetoothManager && bluetoothManager.isCallMuted) ? Qt.darker(ThemeValues.errorCol, 1.3) : Qt.darker(ThemeValues.cardBgCol, 1.5)
                        }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width + 4
                        height: parent.height + 4
                        radius: width / 2
                        color: "transparent"
                        border.width: 1.5
                        border.color: (bluetoothManager && bluetoothManager.isCallMuted) ? Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.3) : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.08)
                    }

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: (bluetoothManager && bluetoothManager.isCallMuted) ? Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.4) : Qt.rgba(0, 0, 0, 0.3)
                        radius: (bluetoothManager && bluetoothManager.isCallMuted) ? 16 : 12
                        samples: 25
                        horizontalOffset: 0
                        verticalOffset: 4
                    }

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
                            if (bluetoothManager) bluetoothManager.toggleMute()
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

                    gradient: Gradient {
                        GradientStop { position: 0.0; color: ThemeValues.errorCol }
                        GradientStop { position: 1.0; color: Qt.darker(ThemeValues.errorCol, 1.3) }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width + 4
                        height: parent.height + 4
                        radius: width / 2
                        color: "transparent"
                        border.width: 1.5
                        border.color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.08)
                    }

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.4)
                        radius: 20
                        samples: 41
                        horizontalOffset: 0
                        verticalOffset: 8
                    }

                    Item {
                        anchors.centerIn: parent
                        width: 56
                        height: 56

                        Canvas {
                            anchors.fill: parent
                            rotation: 135
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.fillStyle = ThemeValues.textCol
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
                            if (bluetoothManager) bluetoothManager.hangupCall()
                        }
                        onPressed: parent.scale = 0.94
                        onReleased: parent.scale = 1.0
                        onCanceled: parent.scale = 1.0
                    }

                    Behavior on scale {
                        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }
                }
            }

            // Device Connection Info
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10
                opacity: 0.4

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u{f293}"
                    font.family: "Font Awesome 6 Free"
                    font.pixelSize: 14
                    color: ThemeValues.textCol
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: getConnectedDeviceName()
                    font.pixelSize: 15
                    font.weight: Font.Normal
                    font.letterSpacing: 0.3
                    font.family: "SF Pro Display"
                    color: ThemeValues.textCol
                }
            }
        }
    ]

    // -------- State Helpers --------

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
        if (!bluetoothManager) return ThemeValues.primaryCol
        var state = bluetoothManager.activeCallState
        switch(state) {
            case "dialing": return ThemeValues.warningCol
            case "alerting": return ThemeValues.warningCol
            case "active": return ThemeValues.successCol
            case "held": return ThemeValues.errorCol
            case "incoming": return ThemeValues.primaryCol
            default: return ThemeValues.primaryCol
        }
    }
}
