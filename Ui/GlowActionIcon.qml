import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

// Centralized action icon component with glow effect (for text-based icons like ♥, ↑, etc.)
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
    property color activeColor: ThemeValues.primaryCol
    property color inactiveColor: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
    property int fontSize: ThemeValues.fontSize

    signal clicked()

    // Internal state
    property bool isPressed: false

    opacity: enabled ? 1.0 : 0.4

    // Glow effect when pressed
    layer.enabled: isPressed
    layer.effect: Glow {
        color: root.activeColor
        spread: 0.5
        radius: 8
        samples: 17
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
        font.family: ThemeValues.fontFamily
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
