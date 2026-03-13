import QtQuick 2.15
import HeadUnit

Item {
    id: root
    property var theme: null
    signal appSelected(string key)

    readonly property var apps: [
        { key: "music",    label: "Music",    icon: "music" },
        { key: "phone",    label: "Phone",    icon: "phone" },
        { key: "messages", label: "Messages", icon: "messages" },
        { key: "contacts", label: "Contacts", icon: "contacts" },
        { key: "weather",  label: "Weather",  icon: "weather" },
        { key: "tidal",    label: "Tidal",    icon: "tidal" },
        { key: "spotify",  label: "Spotify",  icon: "spotify" },
        { key: "vehicle",  label: "Vehicle",  icon: "vehicle" },
        { key: "tuning",   label: "Tuning",   icon: "tuning" },
        { key: "settings", label: "Settings", icon: "settings" }
    ]

    // 5 columns x 2 rows — large touch targets for in-vehicle use
    readonly property int cols: 5
    readonly property int rows: 2
    readonly property real cellSpacing: 16
    readonly property real cellW: (width - cellSpacing * (cols + 1)) / cols
    readonly property real cellH: (height - cellSpacing * (rows + 1)) / rows

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        Grid {
            id: grid
            anchors.centerIn: parent
            columns: root.cols
            spacing: root.cellSpacing

            Repeater {
                model: root.apps

                Rectangle {
                    id: appBg
                    width: root.cellW
                    height: root.cellH
                    color: appMa.pressed
                        ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.25)
                        : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.06)
                    radius: 16
                    border.color: appMa.pressed
                        ? ThemeValues.primaryCol
                        : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 100 } }

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        Image {
                            anchors.horizontalCenter: parent.horizontalCenter
                            source: (root.theme && root.theme.iconPath)
                                ? root.theme.iconPath(modelData.icon) : ""
                            width: 48
                            height: 48
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData.label
                            color: ThemeValues.textCol
                            font.pixelSize: ThemeValues.fontSize + 2
                            font.family: ThemeValues.fontFamily
                            opacity: 0.8
                        }
                    }

                    MouseArea {
                        id: appMa
                        anchors.fill: parent
                        onClicked: root.appSelected(modelData.key)
                    }
                }
            }
        }
    }
}
