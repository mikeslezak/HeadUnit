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

        Text {
            text: "⚠️ Advanced Settings"
            color: ThemeValues.accentCol
            font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
        }

        SettingButton {
            text: "Factory Reset"
            destructive: true
            onClicked: console.log("Factory reset requested")
        }

        SettingButton {
            text: "Clear Cache"
            onClicked: console.log("Cache cleared")
        }

        Rectangle {
            width: parent.width; height: 1
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        }

        Column {
            width: parent.width
            spacing: 8

            Text {
                text: "Developer Info"
                color: ThemeValues.primaryCol
                font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
            }

            InfoRow { label: "Theme"; value: theme?.name ?? "Unknown" }
            InfoRow { label: "Text-to-Speech"; value: voiceAssistant.hasTextToSpeech ? "Available" : "Not Available" }
            InfoRow { label: "Media Connected"; value: mediaController.isConnected ? "Yes" : "No" }
        }
    }

    // --- Inline Components ---

    component SettingButton: Rectangle {
        width: parent.width; height: 48
        color: destructive ? Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.2) : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        border.color: destructive ? ThemeValues.errorCol : ThemeValues.primaryCol; border.width: 2; radius: 8
        property string text: ""; property bool destructive: false
        signal clicked()
        Text { anchors.centerIn: parent; text: parent.text; color: destructive ? ThemeValues.errorCol : ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; font.weight: Font.Bold }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
    }

    component InfoRow: Row {
        width: parent.width; spacing: 12
        property string label: ""; property string value: ""
        Text { text: label + ":"; color: ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; width: 120 }
        Text { text: value; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily }
    }
}
