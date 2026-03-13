import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

/**
 * IncomingCallOverlay - Premium Incoming Call UI
 *
 * Displays incoming call with accept/reject buttons.
 * Appears when receiving an incoming phone call via Bluetooth HFP.
 */
CallOverlayBase {
    id: root

    // -------- Visibility Control --------
    visible: bluetoothManager ? (bluetoothManager.hasActiveCall && bluetoothManager.activeCallState === "incoming") : false
    opacity: visible ? 1.0 : 0.0

    // -------- Fixed incoming state --------
    badgeText: "Incoming Call"
    accentColor: ThemeValues.primaryCol
    badgePulse: true

    // -------- Incoming Call Content --------
    contentItem.children: [
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 20

            // Pulsing phone icon
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 120
                height: 120

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height
                    radius: width / 2
                    color: "transparent"
                    border.width: 2
                    border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)

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

                Canvas {
                    anchors.centerIn: parent
                    width: 64; height: 64
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = ThemeValues.primaryCol.toString()
                        ctx.beginPath()
                        ctx.roundedRect(12, 4, 16, 12, 6, 6)
                        ctx.fill()
                        ctx.beginPath()
                        ctx.roundedRect(12, 48, 16, 12, 6, 6)
                        ctx.fill()
                        ctx.beginPath()
                        ctx.arc(32, 32, 5, 0, Math.PI * 2)
                        ctx.fill()
                    }
                    opacity: 0.9
                }
            }

            // Device info
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: getDeviceName()
                font.pixelSize: 18
                font.weight: Font.Normal
                font.letterSpacing: 0.5
                font.family: ThemeValues.fontFamily
                color: ThemeValues.textCol
                opacity: 0.5
            }

            Item { width: 1; height: 10 }

            // Accept / Reject buttons
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 80

                // Reject Button (Red)
                Rectangle {
                    id: rejectButton
                    width: 160
                    height: 160
                    radius: 80

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
                        border.color: Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.3)
                    }

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.4)
                        radius: 20
                        samples: 41
                        horizontalOffset: 0
                        verticalOffset: 8
                    }

                    Canvas {
                        anchors.centerIn: parent
                        width: 56; height: 56
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = ThemeValues.textCol.toString()
                            ctx.lineWidth = 5
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.moveTo(14, 14); ctx.lineTo(42, 42)
                            ctx.moveTo(42, 14); ctx.lineTo(14, 42)
                            ctx.stroke()
                        }
                        opacity: 0.9
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

                // Accept Button (Green)
                Rectangle {
                    id: acceptButton
                    width: 200
                    height: 200
                    radius: 100

                    gradient: Gradient {
                        GradientStop { position: 0.0; color: ThemeValues.successCol }
                        GradientStop { position: 1.0; color: Qt.darker(ThemeValues.successCol, 1.3) }
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width + 4
                        height: parent.height + 4
                        radius: width / 2
                        color: "transparent"
                        border.width: 1.5
                        border.color: Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.3)
                    }

                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.4)
                        radius: 24
                        samples: 49
                        horizontalOffset: 0
                        verticalOffset: 10
                    }

                    Item {
                        anchors.centerIn: parent
                        width: 64
                        height: 64

                        Canvas {
                            anchors.fill: parent
                            rotation: 0
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.fillStyle = ThemeValues.textCol
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
                            if (bluetoothManager) bluetoothManager.answerCall()
                        }
                        onPressed: parent.scale = 0.94
                        onReleased: parent.scale = 1.0
                        onCanceled: parent.scale = 1.0
                    }

                    Behavior on scale {
                        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                    }

                    SequentialAnimation on scale {
                        running: root.visible
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.05; duration: 1000; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
                    }
                }
            }

            // Action Labels
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 80

                Text {
                    width: 160
                    horizontalAlignment: Text.AlignHCenter
                    text: "Decline"
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    font.letterSpacing: 0.5
                    font.family: ThemeValues.fontFamily
                    color: ThemeValues.textCol
                    opacity: 0.6
                }

                Text {
                    width: 200
                    horizontalAlignment: Text.AlignHCenter
                    text: "Answer"
                    font.pixelSize: 18
                    font.weight: Font.Medium
                    font.letterSpacing: 0.5
                    font.family: ThemeValues.fontFamily
                    color: ThemeValues.textCol
                    opacity: 0.7
                }
            }
        }
    ]

    // -------- Helper --------

    function getDeviceName() {
        if (!bluetoothManager) return ""
        var address = bluetoothManager.getFirstConnectedDeviceAddress()
        if (address) {
            var name = bluetoothManager.getDeviceName(address)
            return name ? "via " + name : "via Bluetooth"
        }
        return "via Bluetooth"
    }
}
