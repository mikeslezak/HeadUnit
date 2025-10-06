import QtQuick 2.15
import QtQuick.Effects

// Centralized button component with glow effect
Rectangle {
    id: root
    width: 64
    height: 64
    color: "transparent"

    // API
    property string iconKey: ""
    property url iconSource: ""
    property bool enabled: true
    property var theme
    property color glowColor: theme?.palette?.primary ?? "#00f0ff"
    property color iconColor: theme?.palette?.primary ?? "#00f0ff"
    property int iconSize: 56

    signal clicked()

    // Internal state
    property bool isPressed: false

    opacity: enabled ? 1.0 : 0.4

    // Glow effect when pressed
    layer.enabled: isPressed
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: root.glowColor
        shadowBlur: 1.0
        shadowOpacity: 0.9
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 0
    }

    // Brightness feedback
    Behavior on opacity {
        NumberAnimation { duration: 100 }
    }

    Image {
        id: icon
        anchors.centerIn: parent
        source: {
            if (root.iconSource !== "") return root.iconSource
            if (root.theme && root.theme.iconPath && root.iconKey !== "") {
                return root.theme.iconPath(root.iconKey)
            }
            return ""
        }
        width: root.iconSize
        height: root.iconSize
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: source !== ""

        layer.enabled: true
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: root.iconColor
        }
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
