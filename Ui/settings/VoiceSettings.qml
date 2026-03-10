import QtQuick 2.15
import HeadUnit

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: col.height
    clip: true

    property var theme: null
    property var appSettings: null

    Column {
        id: col
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        spacing: 20
        width: parent.width - 40

        SettingToggle {
            title: "Auto-Read Messages"
            description: "Read incoming messages aloud while driving"
            isOn: appSettings?.autoReadMessages ?? true
            onToggled: {
                if (appSettings) {
                    appSettings.autoReadMessages = !appSettings.autoReadMessages
                    voiceAssistant.setAutoReadMessages(appSettings.autoReadMessages)
                }
            }
        }

        Rectangle {
            width: parent.width; height: 1
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        }

        Column {
            width: parent.width
            spacing: 8

            Text {
                text: "Voice Volume"
                color: ThemeValues.primaryCol
                font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
            }

            Row {
                spacing: 12; width: parent.width

                Text {
                    text: "🔉"; font.pixelSize: 20; color: ThemeValues.primaryCol
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: parent.width - 100; height: 8
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                    radius: 4; anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        width: parent.width * ((appSettings?.voiceVolume ?? 80) / 100)
                        height: parent.height; color: ThemeValues.primaryCol; radius: 4
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (appSettings) {
                                appSettings.voiceVolume = Math.floor((mouse.x / width) * 100)
                                voiceAssistant.setVoiceVolume(appSettings.voiceVolume)
                            }
                        }
                    }
                }

                Text {
                    text: (appSettings?.voiceVolume ?? 80) + "%"
                    color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Rectangle {
            width: parent.width; height: 1
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        }

        Text {
            text: "Quick Replies"
            color: ThemeValues.primaryCol
            font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
        }

        Column {
            width: parent.width
            spacing: 8

            Repeater {
                model: voiceAssistant.quickReplies.slice(0, 3)

                Rectangle {
                    width: parent.width; height: 40
                    color: Qt.rgba(0, 0, 0, 0.3)
                    border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.5)
                    border.width: 1; radius: 6

                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily
                    }
                }
            }
        }

        Rectangle {
            width: parent.width; height: 1
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        }

        Text {
            text: "Google TTS Settings"
            color: ThemeValues.primaryCol
            font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
        }

        // Voice Selection
        Column {
            width: parent.width
            spacing: 8

            Text {
                text: "Voice Type"
                color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; opacity: 0.8
            }

            Row {
                spacing: 12; width: parent.width

                VoiceButton {
                    text: "Female (US)"; voiceName: "en-US-Neural2-F"
                    isActive: (appSettings?.ttsVoice ?? "en-US-Neural2-F") === "en-US-Neural2-F"
                }

                VoiceButton {
                    text: "Male (US)"; voiceName: "en-US-Neural2-J"
                    isActive: (appSettings?.ttsVoice ?? "en-US-Neural2-F") === "en-US-Neural2-J"
                }
            }

            Row {
                spacing: 12; width: parent.width

                VoiceButton {
                    text: "Female (UK)"; voiceName: "en-GB-Neural2-A"
                    isActive: (appSettings?.ttsVoice ?? "en-US-Neural2-F") === "en-GB-Neural2-A"
                }

                VoiceButton {
                    text: "Male (UK)"; voiceName: "en-GB-Neural2-B"
                    isActive: (appSettings?.ttsVoice ?? "en-US-Neural2-F") === "en-GB-Neural2-B"
                }
            }
        }

        // Speaking Rate
        Column {
            width: parent.width
            spacing: 8

            Text {
                text: "Speaking Speed"
                color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; opacity: 0.8
            }

            Row {
                spacing: 12; width: parent.width

                Text {
                    text: "🐢"; font.pixelSize: 20; color: ThemeValues.primaryCol
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: parent.width - 120; height: 8
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                    radius: 4; anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        width: parent.width * ((appSettings?.ttsSpeakingRate ?? 1.0) - 0.5) / 1.5
                        height: parent.height; color: ThemeValues.primaryCol; radius: 4
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (appSettings) {
                                var rate = 0.5 + (mouse.x / width) * 1.5
                                appSettings.ttsSpeakingRate = Math.round(rate * 10) / 10
                                googleTTS.setSpeakingRate(appSettings.ttsSpeakingRate)
                            }
                        }
                    }
                }

                Text {
                    text: "🐇"; font.pixelSize: 20; color: ThemeValues.primaryCol
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: (appSettings?.ttsSpeakingRate ?? 1.0).toFixed(1) + "x"
                    color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Pitch
        Column {
            width: parent.width
            spacing: 8

            Text {
                text: "Voice Pitch"
                color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; opacity: 0.8
            }

            Row {
                spacing: 12; width: parent.width

                Text {
                    text: "🔽"; font.pixelSize: 20; color: ThemeValues.primaryCol
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: parent.width - 120; height: 8
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                    radius: 4; anchors.verticalCenter: parent.verticalCenter

                    Rectangle {
                        width: parent.width * ((appSettings?.ttsPitch ?? 0.0) + 10.0) / 20.0
                        height: parent.height; color: ThemeValues.primaryCol; radius: 4
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (appSettings) {
                                var pitch = -10.0 + (mouse.x / width) * 20.0
                                appSettings.ttsPitch = Math.round(pitch)
                                googleTTS.setPitch(appSettings.ttsPitch)
                            }
                        }
                    }
                }

                Text {
                    text: "🔼"; font.pixelSize: 20; color: ThemeValues.primaryCol
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: (appSettings?.ttsPitch ?? 0.0) > 0 ? "+" + (appSettings?.ttsPitch ?? 0).toString() : (appSettings?.ttsPitch ?? 0).toString()
                    color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    // --- Inline Components ---

    component SettingToggle: Rectangle {
        width: parent.width; height: 60; color: "transparent"
        property string title: ""; property string description: ""; property bool isOn: false
        signal toggled()
        Column {
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; spacing: 4; width: parent.width - 80
            Text { text: title; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize + 1; font.family: ThemeValues.fontFamily; font.weight: Font.Bold }
            Text { text: description; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily; opacity: 0.6; wrapMode: Text.WordWrap; width: parent.width }
        }
        Rectangle {
            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            width: 60; height: 30; color: isOn ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
            radius: 15; border.color: isOn ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5); border.width: 2
            Rectangle { width: 22; height: 22; radius: 11; color: "white"; x: isOn ? parent.width - width - 4 : 4; anchors.verticalCenter: parent.verticalCenter; Behavior on x { NumberAnimation { duration: 150 } } }
            MouseArea { anchors.fill: parent; onClicked: parent.parent.toggled() }
        }
    }

    component VoiceButton: Rectangle {
        width: (parent.width - 12) / 2; height: 48
        color: isActive ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3) : Qt.rgba(0, 0, 0, 0.3)
        border.color: isActive ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.5)
        border.width: isActive ? 2 : 1; radius: 8
        property string text: ""; property string voiceName: ""; property bool isActive: false
        Text { anchors.centerIn: parent; text: parent.text; color: isActive ? ThemeValues.primaryCol : ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; font.weight: isActive ? Font.Bold : Font.Normal }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (appSettings) {
                    appSettings.ttsVoice = voiceName
                    googleTTS.setVoiceName(voiceName)
                }
            }
        }
    }
}
