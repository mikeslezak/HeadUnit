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
        spacing: 16
        width: parent.width - 40

        Text {
            text: "Select Theme"
            color: ThemeValues.primaryCol
            font.pixelSize: ThemeValues.fontSize + 2
            font.family: ThemeValues.fontFamily
            font.weight: Font.Bold
        }

        Row {
            spacing: 16
            anchors.horizontalCenter: parent.horizontalCenter

            ThemeOption {
                themeName: "Cyberpunk"
                description: "Neon Future"
                isActive: theme?.name === "Cyberpunk"
                onClicked: {
                    if (theme && theme.load) theme.load("Cyberpunk")
                }
            }

            ThemeOption {
                themeName: "RetroVFD"
                description: "Classic Display"
                isActive: theme?.name === "RetroVFD"
                onClicked: {
                    if (theme && theme.load) theme.load("RetroVFD")
                }
            }

            ThemeOption {
                themeName: "Monochrome"
                description: "Luxury Minimal"
                isActive: theme?.name === "Monochrome"
                onClicked: {
                    if (theme && theme.load) theme.load("Monochrome")
                }
            }
        }
    }

    component ThemeOption: Rectangle {
        width: 200; height: 80
        color: isActive ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2) : Qt.rgba(0, 0, 0, 0.3)
        border.color: isActive ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.5)
        border.width: 2; radius: 8

        property string themeName: ""
        property string description: ""
        property bool isActive: false
        signal clicked()

        Column {
            anchors.centerIn: parent; spacing: 4
            Text {
                text: themeName
                color: isActive ? ThemeValues.primaryCol : ThemeValues.textCol
                font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: description
                color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily; opacity: 0.6
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
    }
}
