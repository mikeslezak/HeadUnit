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
            title: "24-Hour Format"
            description: "Use 24-hour time format"
            isOn: appSettings?.use24HourFormat ?? false
            onToggled: {
                if (appSettings) appSettings.use24HourFormat = !appSettings.use24HourFormat
            }
        }

        Rectangle {
            width: parent.width; height: 1
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        }

        Row {
            width: parent.width
            spacing: 12

            Text {
                text: "Current Time:"
                color: ThemeValues.primaryCol
                font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                id: clockText
                text: Qt.formatDateTime(new Date(), (appSettings?.use24HourFormat ?? false) ? "HH:mm:ss" : "h:mm:ss AP")
                color: ThemeValues.textCol
                font.pixelSize: ThemeValues.fontSize + 4; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
                anchors.verticalCenter: parent.verticalCenter

                Timer {
                    interval: 1000
                    running: root.visible
                    repeat: true
                    onTriggered: clockText.text = Qt.formatDateTime(new Date(), (appSettings?.use24HourFormat ?? false) ? "HH:mm:ss" : "h:mm:ss AP")
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
            Rectangle { width: 22; height: 22; radius: 11; color: ThemeValues.textCol; x: isOn ? parent.width - width - 4 : 4; anchors.verticalCenter: parent.verticalCenter; Behavior on x { NumberAnimation { duration: 150 } } }
            MouseArea { anchors.fill: parent; onClicked: parent.parent.toggled() }
        }
    }
}
