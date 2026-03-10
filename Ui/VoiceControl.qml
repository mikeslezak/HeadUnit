import QtQuick 2.15
import QtQuick.Effects
import HeadUnit

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.9)

    property var theme: null
    signal closeRequested()


    // Click anywhere to close
    MouseArea {
        anchors.fill: parent
        onClicked: root.closeRequested()
    }

    Column {
        anchors.centerIn: parent
        spacing: 30

        // Microphone animation
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 120
            height: 120
            radius: 60
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
            border.color: ThemeValues.primaryCol
            border.width: 3

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: ThemeValues.primaryCol
                shadowBlur: 1.0
                shadowOpacity: 0.8
            }

            Text {
                anchors.centerIn: parent
                text: "🎤"
                font.pixelSize: 60
            }

            // Pulsing animation
            SequentialAnimation on scale {
                running: voiceAssistant.isListening
                loops: Animation.Infinite
                NumberAnimation { to: 1.15; duration: 800; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
            }
        }

        // Status text
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: voiceAssistant.isListening ? "Listening..." : "Voice Assistant"
            color: ThemeValues.primaryCol
            font.pixelSize: ThemeValues.fontSize + 10
            font.family: ThemeValues.fontFamily
            font.weight: Font.Bold

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: ThemeValues.primaryCol
                shadowBlur: 0.5
                shadowOpacity: 0.8
            }
        }

        // Command hint
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: voiceAssistant.lastCommand || "Say a command..."
            color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.7)
            font.pixelSize: ThemeValues.fontSize + 2
            font.family: ThemeValues.fontFamily
            width: 400
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        // Quick reply buttons (if available)
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 12
            visible: voiceAssistant.quickReplies.length > 0

            Repeater {
                model: voiceAssistant.quickReplies

                Rectangle {
                    width: 100
                    height: 40
                    radius: 6
                    color: "transparent"
                    border.color: ThemeValues.primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: ThemeValues.primaryCol
                        font.pixelSize: ThemeValues.fontSize - 2
                        font.family: ThemeValues.fontFamily
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            voiceAssistant.sendQuickReply(modelData)
                            root.closeRequested()
                        }
                    }
                }
            }
        }

        // Cancel button
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 140
            height: 45
            radius: 8
            color: "transparent"
            border.color: ThemeValues.accentCol
            border.width: 2

            Text {
                anchors.centerIn: parent
                text: "Cancel"
                color: ThemeValues.accentCol
                font.pixelSize: ThemeValues.fontSize + 2
                font.family: ThemeValues.fontFamily
                font.weight: Font.Bold
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.closeRequested()
            }
        }
    }

    // Fade in animation
    PropertyAnimation {
        target: root
        property: "opacity"
        from: 0
        to: 1
        duration: 200
        easing.type: Easing.OutCubic
        running: true
    }
}
