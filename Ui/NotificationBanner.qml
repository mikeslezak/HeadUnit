import QtQuick 2.15
import HeadUnit

Item {
    id: root

    // ---------- API ----------
    property var theme: null
    signal dismissed()
    signal actionClicked(string action)

    // ---------- Internal State ----------
    property var currentNotification: null
    property bool isShowing: false

    // Auto-dismiss timer
    Timer {
        id: dismissTimer
        interval: 5000
        onTriggered: hideBanner()
    }

    // ---------- Public Methods ----------
    function showNotification(notification) {
        currentNotification = notification
        isShowing = true
        dismissTimer.restart()
    }

    function hideBanner() {
        isShowing = false
        dismissTimer.stop()
        root.dismissed()
    }

    // Banner container
    Rectangle {
        id: banner
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: isShowing ? 20 : -height
        width: Math.min(600, parent.width - 40)
        height: 120
        radius: 16
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.95)
        border.color: ThemeValues.primaryCol
        border.width: 2
        z: 9999

        // Smooth slide-in animation
        Behavior on anchors.topMargin {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }

        // Drop shadow effect (disabled - Qt 6 requires precompiled shaders)
        // layer.enabled: true
        // layer.effect: MultiEffect { shadowEnabled: true; shadowBlur: 0.3 }

        // Click to dismiss
        MouseArea {
            anchors.fill: parent
            onClicked: {} // Prevent click-through
        }

        Row {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 16

            // App Icon
            Rectangle {
                width: 60
                height: 60
                radius: 30
                color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                border.color: ThemeValues.primaryCol
                border.width: 2
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: currentNotification && currentNotification.appName ?
                          currentNotification.appName.substring(0, 1).toUpperCase() : "?"
                    color: ThemeValues.primaryCol
                    font.pixelSize: 28
                    font.family: ThemeValues.fontFamily
                    font.weight: Font.Bold
                }
            }

            // Content
            Column {
                width: parent.width - 180
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                // App Name + Time
                Row {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: currentNotification ? (currentNotification.appName || "Notification") : ""
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.6)
                        font.pixelSize: ThemeValues.fontSize - 4
                        font.family: ThemeValues.fontFamily
                        font.weight: Font.Medium
                    }

                    Text {
                        text: "now"
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.4)
                        font.pixelSize: ThemeValues.fontSize - 4
                        font.family: ThemeValues.fontFamily
                    }
                }

                // Title
                Text {
                    text: currentNotification ? (currentNotification.title || "") : ""
                    color: ThemeValues.textCol
                    font.pixelSize: ThemeValues.fontSize + 2
                    font.family: ThemeValues.fontFamily
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    width: parent.width
                }

                // Message
                Text {
                    text: currentNotification ? (currentNotification.message || "") : ""
                    color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.85)
                    font.pixelSize: ThemeValues.fontSize
                    font.family: ThemeValues.fontFamily
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    width: parent.width
                }
            }

            // Actions
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                // Notification-specific actions
                Repeater {
                    model: currentNotification && currentNotification.actions
                        ? currentNotification.actions : []

                    Rectangle {
                        width: 70
                        height: 30
                        radius: 15
                        color: "transparent"
                        border.color: ThemeValues.primaryCol
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: ThemeValues.primaryCol
                            font.pixelSize: ThemeValues.fontSize - 3
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (currentNotification) {
                                    notificationManager.performAction(
                                        currentNotification.id || "", modelData)
                                }
                                hideBanner()
                            }
                        }
                    }
                }

                // View button (always shown)
                Rectangle {
                    width: 70
                    height: 30
                    radius: 15
                    color: "transparent"
                    border.color: ThemeValues.primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "View"
                        color: ThemeValues.primaryCol
                        font.pixelSize: ThemeValues.fontSize - 2
                        font.family: ThemeValues.fontFamily
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.actionClicked("view")
                            hideBanner()
                        }
                    }
                }

                // Close button
                Rectangle {
                    width: 70
                    height: 30
                    radius: 15
                    color: "transparent"
                    border.color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.6)
                        font.pixelSize: ThemeValues.fontSize + 2
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: hideBanner()
                    }
                }
            }
        }

        // Subtle glow effect for urgent notifications
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: parent.radius + 2
            color: "transparent"
            border.color: ThemeValues.accentCol
            border.width: currentNotification && currentNotification.priority === 3 ? 2 : 0
            opacity: 0.6
            z: -1

            SequentialAnimation on opacity {
                running: currentNotification && currentNotification.priority === 3
                loops: Animation.Infinite
                NumberAnimation { to: 0.2; duration: 800 }
                NumberAnimation { to: 0.6; duration: 800 }
            }
        }
    }
}
