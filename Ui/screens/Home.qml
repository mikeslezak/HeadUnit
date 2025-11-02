import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Effects

Item {
    id: root
    property var theme: null

    signal appSelected(string key)

    readonly property color bgCol: (theme && theme.palette && theme.palette.bg) ? theme.palette.bg : "#0a0a0f"
    readonly property color textCol: (theme && theme.palette && theme.palette.text) ? theme.palette.text : "white"
    readonly property color primaryCol: (theme && theme.palette && theme.palette.primary) ? theme.palette.primary : "#00f0ff"
    readonly property string fontFamily: (theme && theme.typography && theme.typography.fontFamily) ? theme.typography.fontFamily : "Noto Sans"
    readonly property int fontSize: (theme && theme.typography && theme.typography.fontSize) ? Number(theme.typography.fontSize) : 16
    readonly property int iconSize: 56

    Rectangle {
        anchors.fill: parent
        color: bgCol

        SwipeView {
            id: swipeView
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.topMargin: 40
            anchors.bottomMargin: 60

            // Page 1 - Main Apps
            Item {
                Grid {
                    anchors.centerIn: parent
                    columns: 4
                    rows: 2
                    columnSpacing: 32
                    rowSpacing: 32

                    // Maps
                    AppIcon {
                        appKey: "maps"
                        appName: "Maps"
                        iconKey: "maps"
                    }

                    // Phone
                    AppIcon {
                        appKey: "phone"
                        appName: "Phone"
                        iconKey: "phone"
                    }

                    // Messages
                    AppIcon {
                        appKey: "messages"
                        appName: "Messages"
                        iconKey: "messages"
                    }

                    // Music (Bluetooth)
                    AppIcon {
                        appKey: "music"
                        appName: "Music"
                        iconKey: "music"
                    }

                    // Tidal
                    AppIcon {
                        appKey: "tidal"
                        appName: "Tidal"
                        iconKey: "tidal"
                    }

                    // Spotify
                    AppIcon {
                        appKey: "spotify"
                        appName: "Spotify"
                        iconKey: "spotify"
                    }

                    // Radio
                    AppIcon {
                        appKey: "radio"
                        appName: "Radio"
                        iconKey: "radio"
                    }

                    // Podcasts
                    AppIcon {
                        appKey: "podcasts"
                        appName: "Podcasts"
                        iconKey: "podcasts"
                    }
                }
            }

            // Page 2 - Vehicle & Info
            Item {
                Grid {
                    anchors.centerIn: parent
                    columns: 4
                    rows: 2
                    columnSpacing: 32
                    rowSpacing: 32

                    // Climate
                    AppIcon {
                        appKey: "climate"
                        appName: "Climate"
                        iconKey: "climate"
                    }

                    // Vehicle
                    AppIcon {
                        appKey: "vehicle"
                        appName: "Vehicle"
                        iconKey: "vehicle"
                    }

                    // Camera
                    AppIcon {
                        appKey: "camera"
                        appName: "Camera"
                        iconKey: "camera"
                    }

                    // Contacts
                    AppIcon {
                        appKey: "contacts"
                        appName: "Contacts"
                        iconKey: "contacts"
                    }

                    // Weather
                    AppIcon {
                        appKey: "weather"
                        appName: "Weather"
                        iconKey: "weather"
                    }

                    // Settings
                    AppIcon {
                        appKey: "settings"
                        appName: "Settings"
                        iconKey: "settings"
                    }

                    // Placeholder
                    PlaceholderIcon { placeholderText: "Coming" }
                    PlaceholderIcon { placeholderText: "Soon" }
                }
            }
        }

        // Page indicator dots
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            spacing: 8

            Repeater {
                model: swipeView.count
                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: swipeView.currentIndex === index ? primaryCol : textCol
                    opacity: swipeView.currentIndex === index ? 1.0 : 0.3
                    Behavior on opacity { NumberAnimation { duration: 200 } }
                }
            }
        }
    }

    // Reusable App Icon Component
    component AppIcon: Item {
        width: 100
        height: 110

        required property string appKey
        required property string appName
        required property string iconKey

        Column {
            anchors.centerIn: parent
            spacing: 12

            Rectangle {
                width: root.iconSize + 16
                height: root.iconSize + 16
                color: "transparent"
                radius: 14
                anchors.horizontalCenter: parent.horizontalCenter

                Image {
                    anchors.centerIn: parent
                    source: theme && theme.iconPath ? theme.iconPath(iconKey) : ""
                    width: root.iconSize
                    height: root.iconSize
                    sourceSize.width: root.iconSize
                    sourceSize.height: root.iconSize
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    antialiasing: true
                    visible: true
                    cache: true
                    asynchronous: false

                }
            }

            Text {
                text: appName
                color: textCol
                font.pixelSize: fontSize
                font.family: fontFamily
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        transform: Scale {
            id: appScale
            origin.x: 50
            origin.y: 55
            xScale: 1
            yScale: 1
            Behavior on xScale { NumberAnimation { duration: 100 } }
            Behavior on yScale { NumberAnimation { duration: 100 } }
        }

        MouseArea {
            anchors.fill: parent
            anchors.margins: -10
            onPressed: { appScale.xScale = 0.92; appScale.yScale = 0.92 }
            onReleased: { appScale.xScale = 1; appScale.yScale = 1 }
            onClicked: root.appSelected(appKey)
        }
    }

    // Placeholder Icon Component
    component PlaceholderIcon: Item {
        width: 100
        height: 110
        opacity: 0.3

        required property string placeholderText

        Column {
            anchors.centerIn: parent
            spacing: 12

            Rectangle {
                width: root.iconSize + 16
                height: root.iconSize + 16
                color: primaryCol
                opacity: 0.1
                radius: 14
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: placeholderText
                color: textCol
                font.pixelSize: fontSize - 2
                font.family: fontFamily
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
