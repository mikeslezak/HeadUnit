import QtQuick 2.15
import QtQuick.Effects

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.9)

    required property var theme
    signal closeRequested()

    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color accentCol: theme?.palette?.accent ?? "#ff006e"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16

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
            color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
            border.color: primaryCol
            border.width: 3

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: primaryCol
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

        // Test input field (temporary - for testing without microphone)
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 500
            height: 50
            radius: 8
            color: Qt.rgba(0, 0, 0, 0.5)
            border.color: primaryCol
            border.width: 2

            TextInput {
                id: testCommandInput
                anchors.fill: parent
                anchors.margins: 12
                color: textCol
                font.pixelSize: fontSize + 2
                font.family: fontFamily
                verticalAlignment: TextInput.AlignVCenter
                selectByMouse: true

                Text {
                    anchors.centerIn: parent
                    text: "Type command here (e.g., 'call mom')"
                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                    font.pixelSize: fontSize
                    font.family: fontFamily
                    visible: testCommandInput.text.length === 0
                }

                Keys.onReturnPressed: {
                    if (testCommandInput.text.length > 0) {
                        console.log("Test command:", testCommandInput.text)
                        // Send to Claude for processing
                        claudeClient.sendMessage(testCommandInput.text)
                        testCommandInput.text = ""
                    }
                }
            }
        }

        // Status text
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: voiceAssistant.isListening ? "Listening..." : "Voice Assistant (Test Mode)"
            color: primaryCol
            font.pixelSize: fontSize + 10
            font.family: fontFamily
            font.weight: Font.Bold

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: primaryCol
                shadowBlur: 0.5
                shadowOpacity: 0.8
            }
        }

        // Command hint
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: voiceAssistant.lastCommand || "Say a command..."
            color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.7)
            font.pixelSize: fontSize + 2
            font.family: fontFamily
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
                    border.color: primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: primaryCol
                        font.pixelSize: fontSize - 2
                        font.family: fontFamily
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
            border.color: accentCol
            border.width: 2

            Text {
                anchors.centerIn: parent
                text: "Cancel"
                color: accentCol
                font.pixelSize: fontSize + 2
                font.family: fontFamily
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
