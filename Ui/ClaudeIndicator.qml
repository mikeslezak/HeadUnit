import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

/**
 * ClaudeIndicator - Siri-like glowing shimmer overlay for Claude AI
 *
 * Full-screen glowing gradient overlay that shimmers when Claude is active
 */
Rectangle {
    id: root
    anchors.fill: parent
    color: ThemeValues.bgCol

    // -------- API --------
    property var theme: null  // Optional, not required
    signal closeRequested()

    property string state: "listening"  // "listening", "processing", "speaking"
    property bool isActive: false

    // -------- State Colors (derived from theme) --------
    readonly property color listeningColor: ThemeValues.primaryCol
    readonly property color processingColor: ThemeValues.accentCol
    readonly property color speakingColor: Qt.lighter(ThemeValues.primaryCol, 1.4)

    readonly property color currentColor: {
        switch(state) {
            case "listening": return listeningColor
            case "processing": return processingColor
            case "speaking": return speakingColor
            default: return listeningColor
        }
    }

    // -------- Semi-transparent background --------
    opacity: 0.0
    visible: opacity > 0

    // Smooth fade animations
    Behavior on opacity {
        NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
    }

    // -------- Tap to dismiss --------
    MouseArea {
        anchors.fill: parent
        onClicked: root.closeRequested()
    }

    // -------- Glowing orb/shimmer effect --------
    Item {
        id: glowContainer
        anchors.centerIn: parent
        width: 600
        height: 600

        // Multiple overlapping circles for shimmer effect
        Repeater {
            model: 5

            Rectangle {
                id: glowCircle
                anchors.centerIn: parent
                width: 300 + (index * 60)
                height: width
                radius: width / 2
                color: "transparent"
                border.width: 0

                // Gradient fill
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: Qt.rgba(root.currentColor.r, root.currentColor.g, root.currentColor.b, 0.6 - index * 0.1)
                    }
                    GradientStop {
                        position: 1.0
                        color: Qt.rgba(root.currentColor.r, root.currentColor.g, root.currentColor.b, 0.0)
                    }
                }

                // Glow effect
                layer.enabled: true
                layer.effect: Glow {
                    color: root.currentColor
                    spread: 0.3
                    radius: 24 + (index * 8)
                    samples: 25
                }

                // Pulsing animation - each circle pulses at different rate
                SequentialAnimation on scale {
                    running: root.isActive
                    loops: Animation.Infinite

                    PauseAnimation {
                        duration: index * 100
                    }

                    NumberAnimation {
                        to: 1.3 + (index * 0.1)
                        duration: 1200 + (index * 200)
                        easing.type: Easing.InOutSine
                    }

                    NumberAnimation {
                        to: 0.9 - (index * 0.05)
                        duration: 1200 + (index * 200)
                        easing.type: Easing.InOutSine
                    }
                }

                // Opacity pulsing for shimmer
                SequentialAnimation on opacity {
                    running: root.isActive
                    loops: Animation.Infinite

                    NumberAnimation {
                        to: 0.3
                        duration: 800 + (index * 150)
                        easing.type: Easing.InOutQuad
                    }

                    NumberAnimation {
                        to: 0.8
                        duration: 800 + (index * 150)
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }

        // Center bright spot
        Rectangle {
            id: centerOrb
            anchors.centerIn: parent
            width: 120
            height: 120
            radius: 60
            color: root.currentColor

            layer.enabled: true
            layer.effect: Glow {
                color: root.currentColor
                spread: 0.5
                radius: 32
                samples: 25
            }

            // Breathing animation
            SequentialAnimation on scale {
                running: root.isActive
                loops: Animation.Infinite

                NumberAnimation {
                    to: 1.4
                    duration: 1000
                    easing.type: Easing.InOutSine
                }

                NumberAnimation {
                    to: 0.8
                    duration: 1000
                    easing.type: Easing.InOutSine
                }
            }

            SequentialAnimation on opacity {
                running: root.isActive
                loops: Animation.Infinite

                NumberAnimation {
                    to: 0.4
                    duration: 1000
                    easing.type: Easing.InOutSine
                }

                NumberAnimation {
                    to: 1.0
                    duration: 1000
                    easing.type: Easing.InOutSine
                }
            }
        }
    }

    // -------- Status Text --------
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: glowContainer.bottom
        anchors.topMargin: 60
        width: statusText.width + 40
        height: statusText.height + 20
        radius: height / 2
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.7)
        border.width: 2
        border.color: root.currentColor

        layer.enabled: true
        layer.effect: Glow {
            color: root.currentColor
            spread: 0.2
            radius: 8
            samples: 17
        }

        Text {
            id: statusText
            anchors.centerIn: parent

            text: {
                switch(root.state) {
                    case "listening": return "Listening..."
                    case "processing": return "Thinking..."
                    case "speaking": return "Speaking..."
                    default: return ""
                }
            }

            font.pixelSize: 28
            font.weight: Font.Medium
            font.family: ThemeValues.fontFamily
            color: ThemeValues.textCol

            // Subtle pulsing
            SequentialAnimation on opacity {
                running: root.isActive
                loops: Animation.Infinite
                NumberAnimation { to: 0.6; duration: 1200; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.0; duration: 1200; easing.type: Easing.InOutQuad }
            }
        }
    }

    // -------- Close hint --------
    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter

        text: "Tap anywhere else to cancel"
        font.pixelSize: 16
        font.family: ThemeValues.fontFamily
        color: ThemeValues.textCol
        opacity: 0.5
    }

    // -------- Show/Hide Functions --------
    function show() {
        console.log("ClaudeIndicator: Showing with state:", state)
        isActive = true
        opacity = 0.95
    }

    function hide() {
        console.log("ClaudeIndicator: Hiding - caller trace:", new Error().stack)
        isActive = false
        opacity = 0.0
    }

    // -------- State Management --------
    function setState(newState) {
        console.log("ClaudeIndicator: State changed to:", newState)
        state = newState
    }

    Component.onCompleted: {
        console.log("ClaudeIndicator: Component completed")
    }
}
