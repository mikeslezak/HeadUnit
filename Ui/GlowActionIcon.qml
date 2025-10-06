import QtQuick 2.15
import QtQuick.Effects

// Centralized action icon component with glow effect (for text-based icons like ♥, ↓, etc.)
Rectangle {
    id: root
    width: 56
    height: 56
    color: "transparent"

    // API
    property string iconText: ""
    property bool isActive: false
    property bool enabled: true
    property var theme
    property color activeColor: theme?.palette?.primary ?? "#00f0ff"
    property color inactiveColor: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
    property color textCol: theme?.palette?.text ?? "#39ff14"
    property int fontSize: theme?.typography?.fontSize ?? 16

    signal clicked()

    // Internal state
    property bool isPressed: false

    opacity: enabled ? 1.0 : 0.4

    // Glow effect when pressed
    layer.enabled: isPressed
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: root.activeColor
        shadowBlur: 1.0
        shadowOpacity: 0.9
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
    }

    // Brightness feedback
    Behavior on opacity {
        NumberAnimation { duration: 100 }
    }

    Text {
        anchors.centerIn: parent
        text: root.iconText
        color: root.isActive ? root.activeColor : root.inactiveColor
        font.pixelSize: root.fontSize + 12
        font.weight: Font.Bold
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        onPressed: {
            root.isPressed = true
            root.opacity = 0.7
        }
        onReleased: {
            root.isPressed = false
            root.opacity = root.enabled ? 1.0 : 0.4
        }
        onClicked: root.clicked()
    }
}
