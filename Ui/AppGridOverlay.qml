import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.85)
    visible: false
    opacity: 0

    property var theme: null
    signal appSelected(string key)


    readonly property var apps: [
        { key: "home",     label: "Home",     icon: "home" },
        { key: "music",    label: "Music",    icon: "music" },
        { key: "maps",     label: "Maps",     icon: "maps" },
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

    function show() {
        visible = true
        showAnim.start()
    }

    function hide() {
        hideAnim.start()
    }

    NumberAnimation {
        id: showAnim
        target: root
        property: "opacity"
        from: 0; to: 1
        duration: 200
        easing.type: Easing.OutQuad
    }

    SequentialAnimation {
        id: hideAnim
        NumberAnimation {
            target: root
            property: "opacity"
            from: 1; to: 0
            duration: 150
            easing.type: Easing.InQuad
        }
        ScriptAction { script: root.visible = false }
    }

    // Tap background to close
    MouseArea {
        anchors.fill: parent
        onClicked: root.hide()
    }

    // Grid container
    Rectangle {
        anchors.centerIn: parent
        width: grid.width + 48
        height: grid.height + 48
        color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.95)
        radius: 16
        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        border.width: 1

        // Prevent clicks from closing
        MouseArea { anchors.fill: parent }

        Grid {
            id: grid
            anchors.centerIn: parent
            columns: 4
            spacing: 20

            Repeater {
                model: root.apps

                Item {
                    width: 100
                    height: 100

                    Rectangle {
                        id: appBg
                        anchors.fill: parent
                        color: appMa.pressed
                            ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.06)
                        radius: 12
                        border.color: appMa.pressed
                            ? ThemeValues.primaryCol
                            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                        border.width: 1

                        Behavior on color { ColorAnimation { duration: 100 } }

                        Column {
                            anchors.centerIn: parent
                            spacing: 8

                            Image {
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: (root.theme && root.theme.iconPath)
                                    ? root.theme.iconPath(modelData.icon) : ""
                                width: 36
                                height: 36
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: ThemeValues.textCol
                                font.pixelSize: 12
                                font.family: ThemeValues.fontFamily
                                opacity: 0.8
                            }
                        }
                    }

                    MouseArea {
                        id: appMa
                        anchors.fill: parent
                        onClicked: {
                            root.appSelected(modelData.key)
                            root.hide()
                        }
                    }
                }
            }
        }
    }
}
