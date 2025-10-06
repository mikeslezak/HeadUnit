import QtQuick 2.15

Item {
    id: root

    // ---------- API ----------
    required property var theme

    signal dismissed()
    signal actionClicked(string action)

    // ---------- Theme tokens ----------
    readonly property color bgCol: theme?.palette?.background ?? "#0a0a0f"
    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color accentCol: theme?.palette?.accent ?? "#ff006e"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16

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
        color: Qt.rgba(bgCol.r, bgCol.g, bgCol.b, 0.95)
        border.color: primaryCol
        border.width: 2
        z: 9999

        // Smooth slide-in animation
        Behavior on anchors.topMargin {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }

        // Drop shadow effect
        layer.enabled: true
        layer.effect: Item {
            ShaderEffect {
                property variant source
                fragmentShader: "
                    varying highp vec2 qt_TexCoord0;
                    uniform sampler2D source;
                    uniform lowp float qt_Opacity;
                    void main() {
                        gl_FragColor = texture2D(source, qt_TexCoord0) * qt_Opacity;
                    }
                "
            }
        }

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
                color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                border.color: primaryCol
                border.width: 2
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: currentNotification && currentNotification.appName ?
                          currentNotification.appName.substring(0, 1).toUpperCase() : "?"
                    color: primaryCol
                    font.pixelSize: 28
                    font.family: fontFamily
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
                        color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.6)
                        font.pixelSize: fontSize - 4
                        font.family: fontFamily
                        font.weight: Font.Medium
                    }

                    Text {
                        text: "now"
                        color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.4)
                        font.pixelSize: fontSize - 4
                        font.family: fontFamily
                    }
                }

                // Title
                Text {
                    text: currentNotification ? (currentNotification.title || "") : ""
                    color: textCol
                    font.pixelSize: fontSize + 2
                    font.family: fontFamily
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    width: parent.width
                }

                // Message
                Text {
                    text: currentNotification ? (currentNotification.message || "") : ""
                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.85)
                    font.pixelSize: fontSize
                    font.family: fontFamily
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    width: parent.width
                }
            }

            // Actions
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                // View button
                Rectangle {
                    width: 70
                    height: 36
                    radius: 18
                    color: "transparent"
                    border.color: primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "View"
                        color: primaryCol
                        font.pixelSize: fontSize - 2
                        font.family: fontFamily
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.actionClicked("view")
                            hideBanner()
                        }
                    }
                }

                // Close button
                Rectangle {
                    width: 70
                    height: 36
                    radius: 18
                    color: "transparent"
                    border.color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.3)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.6)
                        font.pixelSize: fontSize + 2
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
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
            border.color: accentCol
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
