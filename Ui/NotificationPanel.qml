import QtQuick 2.15
import QtQuick.Effects

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.85)

    required property var theme
    signal closeRequested()

    property bool isPinned: false

    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color accentCol: theme?.palette?.accent ?? "#ff006e"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16

    // Click outside to close
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (!isPinned) {
                root.closeRequested()
            }
        }
    }

    // Notification panel
    Rectangle {
        id: panel
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 350
        color: bgCol
        border.color: primaryCol
        border.width: 2

        MouseArea {
            anchors.fill: parent
            onClicked: {} // Prevent click-through
        }

        Column {
            anchors.fill: parent
            spacing: 0

            // Header
            Rectangle {
                width: parent.width
                height: 60
                color: Qt.rgba(0, 0, 0, 0.5)
                border.color: primaryCol
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    Text {
                        text: "NOTIFICATIONS"
                        color: primaryCol
                        font.pixelSize: fontSize + 4
                        font.family: fontFamily
                        font.weight: Font.Bold
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Item { Layout.fillWidth: true; width: parent.width - 250 }

                    Rectangle {
                        width: 30
                        height: 30
                        radius: 15
                        color: "transparent"
                        border.color: primaryCol
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: notificationManager.notificationCount
                            color: primaryCol
                            font.pixelSize: fontSize - 2
                            font.family: fontFamily
                            font.weight: Font.Bold
                        }
                    }

                    // Pin button
                    Rectangle {
                        width: 35
                        height: 35
                        radius: 6
                        color: isPinned ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2) : "transparent"
                        border.color: primaryCol
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: isPinned ? "📌" : "📍"
                            font.pixelSize: 18
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: isPinned = !isPinned
                        }
                    }

                    // Close button
                    Rectangle {
                        width: 35
                        height: 35
                        radius: 6
                        color: "transparent"
                        border.color: accentCol
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: accentCol
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.closeRequested()
                        }
                    }
                }
            }

            // Notification list
            ListView {
                width: parent.width
                height: parent.height - 60
                model: notificationManager.notifications
                clip: true
                spacing: 2

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 100
                    color: Qt.rgba(0, 0, 0, 0.3)
                    border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                    border.width: 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 6

                        Row {
                            width: parent.width
                            spacing: 10

                            // App icon/avatar
                            Rectangle {
                                width: 40
                                height: 40
                                radius: 20
                                color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                                border.color: primaryCol
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.appName ? modelData.appName.substring(0, 1).toUpperCase() : "?"
                                    color: primaryCol
                                    font.pixelSize: 20
                                    font.weight: Font.Bold
                                }
                            }

                            Column {
                                width: parent.width - 60
                                spacing: 2

                                Text {
                                    text: modelData.title || "Notification"
                                    color: textCol
                                    font.pixelSize: fontSize
                                    font.family: fontFamily
                                    font.weight: Font.Bold
                                    elide: Text.ElideRight
                                    width: parent.width
                                }

                                Text {
                                    text: modelData.message || ""
                                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.7)
                                    font.pixelSize: fontSize - 3
                                    font.family: fontFamily
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                            }
                        }

                        // Action buttons
                        Row {
                            spacing: 8
                            visible: modelData.actions && modelData.actions.length > 0

                            Repeater {
                                model: modelData.actions || []

                                Rectangle {
                                    width: 70
                                    height: 28
                                    radius: 4
                                    color: "transparent"
                                    border.color: primaryCol
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: primaryCol
                                        font.pixelSize: fontSize - 4
                                        font.family: fontFamily
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            console.log("Action clicked:", modelData)
                                            notificationManager.performAction(index, modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Dismiss button
                    Rectangle {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 8
                        width: 24
                        height: 24
                        radius: 12
                        color: Qt.rgba(accentCol.r, accentCol.g, accentCol.b, 0.2)
                        border.color: accentCol
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: accentCol
                            font.pixelSize: 14
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: notificationManager.dismissNotification(modelData.id)
                        }
                    }
                }

                // Empty state
                Text {
                    anchors.centerIn: parent
                    text: "No notifications"
                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                    font.pixelSize: fontSize + 2
                    font.family: fontFamily
                    visible: notificationManager.notificationCount === 0
                }
            }
        }
    }

    // Slide in animation
    PropertyAnimation {
        target: panel
        property: "anchors.rightMargin"
        from: -panel.width
        to: 0
        duration: 300
        easing.type: Easing.OutCubic
        running: true
    }
}
