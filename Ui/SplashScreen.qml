import QtQuick 2.15
import QtQuick.Effects

Rectangle {
    id: root
    anchors.fill: parent
    required property var theme

    signal finished()

    property int stage: 0
    property string animText: ""
    property bool themeReady: false

    // Theme tokens with comprehensive fallbacks
    readonly property color splashBg: theme?.splash?.bg ?? "#000000"
    readonly property color splashColor: theme?.splash?.color ?? "#00d9d9"
    readonly property color splashGlow: theme?.splash?.glow ?? "#00d9d966"
    readonly property string splashFont: theme?.typography?.fontFamily ?? "Noto Sans"

    // Logo animation: "breathe", "pulse", "glow", "static", or "none"
    readonly property string logoAnim: theme?.splash?.logoAnim ?? "breathe"

    // Text to display (can be different per theme)
    readonly property string primaryText: theme?.splash?.primaryText ?? "CHEVROLET"
    readonly property string secondaryText: theme?.splash?.secondaryText ?? "DELCO ELECTRONICS"

    // Sizing
    readonly property int logoSize: Number(theme?.splash?.logoSize ?? 250)
    readonly property int titleSize: Number(theme?.splash?.titleSize ?? 42)
    readonly property int subtitleSize: Number(theme?.splash?.subtitleSize ?? 18)

    // Timing
    readonly property int charDuration: Number(theme?.splash?.charDuration ?? 100)
    readonly property int holdDuration: Number(theme?.splash?.holdDuration ?? 3000)
    readonly property int fadeOutDuration: Number(theme?.splash?.fadeOutDuration ?? 1000)

    // Effects
    readonly property bool showScanlines: (theme?.splash?.scanlines ?? true)
    readonly property real scanlineOpacity: Number(theme?.splash?.scanlineOpacity ?? 0.15)
    readonly property int scanlineSpacing: Number(theme?.splash?.scanlineSpacing ?? 3)

    // Glow/shadow effects
    readonly property bool useGlow: (theme?.splash?.useGlow ?? true)
    readonly property real glowIntensity: Number(theme?.splash?.glowIntensity ?? 8)

    // Logo color method: "colorize" (tint white SVG) or "native" (use SVG colors)
    readonly property string logoColorMode: theme?.splash?.logoColorMode ?? "colorize"

    color: splashBg

    // Monitor theme loading
    Connections {
        target: theme
        function onThemeChanged() {
            console.log("Splash: Theme changed, marking as ready")
            root.themeReady = true
        }
    }

    Component.onCompleted: {
        console.log("Splash: Component completed")
        if (theme && theme.palette && theme.palette.bg) {
            console.log("Splash: Theme already loaded, starting immediately")
            root.themeReady = true
        } else {
            console.log("Splash: Waiting for theme to load...")
            fallbackTimer.start()
        }
    }

    Timer {
        id: fallbackTimer
        interval: 500
        onTriggered: {
            if (!root.themeReady) {
                console.log("Splash: Fallback timer - forcing start")
                root.themeReady = true
            }
        }
    }

    // Scanlines overlay
    Canvas {
        id: scanlinesCanvas
        anchors.fill: parent
        opacity: 0
        visible: showScanlines
        z: 100

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = root.splashColor
            ctx.lineWidth = 1

            for (var y = 0; y < height; y += root.scanlineSpacing) {
                ctx.beginPath()
                ctx.moveTo(0, y)
                ctx.lineTo(width, y)
                ctx.stroke()
            }
        }

        NumberAnimation on opacity {
            from: scanlineOpacity * 0.8
            to: scanlineOpacity * 1.2
            duration: 2000
            loops: Animation.Infinite
            easing.type: Easing.InOutSine
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    // Main content
    Item {
        id: content
        anchors.centerIn: parent
        width: 600
        height: 400

        // Logo with theme-driven animations
        Item {
            id: logoContainer
            anchors.horizontalCenter: parent.horizontalCenter
            y: 40
            width: root.logoSize
            height: root.logoSize * 0.35
            opacity: 0

            property real animValue: 1.0

            Behavior on opacity {
                NumberAnimation {
                    duration: 800
                    easing.type: Easing.OutCubic
                }
            }

            Component.onCompleted: {
                Qt.callLater(function() {
                    logoContainer.opacity = 1
                })
            }

            // Breathing animation
            SequentialAnimation on animValue {
                running: root.logoAnim === "breathe"
                loops: Animation.Infinite
                PauseAnimation { duration: 500 }
                NumberAnimation {
                    from: 0.95
                    to: 1.0
                    duration: 2000
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 1.0
                    to: 0.95
                    duration: 2000
                    easing.type: Easing.InOutQuad
                }
            }

            // Pulse animation
            SequentialAnimation on animValue {
                running: root.logoAnim === "pulse"
                loops: Animation.Infinite
                NumberAnimation {
                    from: 1.0
                    to: 1.1
                    duration: 300
                    easing.type: Easing.OutQuad
                }
                NumberAnimation {
                    from: 1.1
                    to: 1.0
                    duration: 300
                    easing.type: Easing.InQuad
                }
                PauseAnimation { duration: 1400 }
            }

            // Glow animation
            SequentialAnimation on animValue {
                running: root.logoAnim === "glow"
                loops: Animation.Infinite
                NumberAnimation {
                    from: 0.7
                    to: 1.0
                    duration: 1500
                    easing.type: Easing.InOutSine
                }
                NumberAnimation {
                    from: 1.0
                    to: 0.7
                    duration: 1500
                    easing.type: Easing.InOutSine
                }
            }

            Image {
                id: logo
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: true
                opacity: root.logoAnim === "static" || root.logoAnim === "none" ? 1.0 : logoContainer.animValue
                scale: root.logoAnim === "pulse" ? logoContainer.animValue : 1.0

                Connections {
                    target: root.theme
                    function onThemeChanged() { logo.updateLogoSource() }
                }

                Component.onCompleted: updateLogoSource()

                function updateLogoSource() {
                    if (theme?.splash?.logo && theme?.name) {
                        source = "qrc:/qt/qml/HeadUnit/themes/" + theme.name + "/" + String(theme.splash.logo)
                    }
                }

                layer.enabled: root.logoColorMode === "colorize"
                layer.smooth: false
                layer.effect: MultiEffect {
                    colorization: 1.0
                    colorizationColor: root.splashColor

                    shadowEnabled: root.useGlow
                    shadowBlur: root.glowIntensity / 30.0
                    shadowOpacity: root.logoAnim === "glow" ? logoContainer.animValue : 0.6
                    shadowColor: root.splashColor
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                    shadowScale: 1.02
                }
            }
        }

        // Primary text
        Item {
            id: primaryTextContainer
            anchors.horizontalCenter: parent.horizontalCenter
            y: 200
            width: primaryText.width
            height: primaryText.height
            opacity: root.animText.length > 0 ? 1.0 : 0

            property real breatheAmount: 1.0

            SequentialAnimation on breatheAmount {
                running: true
                loops: Animation.Infinite
                PauseAnimation { duration: 500 }
                NumberAnimation {
                    from: 0.95; to: 1.0
                    duration: 2000
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 1.0; to: 0.95
                    duration: 2000
                    easing.type: Easing.InOutQuad
                }
            }

            Text {
                id: primaryText
                anchors.centerIn: parent
                text: root.animText
                color: root.splashColor
                font.family: root.splashFont
                font.pixelSize: root.titleSize
                font.weight: Font.Bold
                opacity: primaryTextContainer.breatheAmount

                layer.enabled: root.useGlow
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: root.glowIntensity / 30.0
                    shadowOpacity: 0.8
                    shadowColor: root.splashGlow
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                }
            }
        }

        // Blinking cursor
        Text {
            visible: root.animText.length > 0 && root.animText.length < root.primaryText.length
            anchors.horizontalCenter: parent.horizontalCenter
            x: primaryTextContainer.x + primaryText.width + 4
            y: primaryTextContainer.y
            text: "_"
            color: root.splashColor
            font.family: root.splashFont
            font.pixelSize: root.titleSize

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.0; duration: 400 }
                NumberAnimation { from: 0.0; to: 1.0; duration: 400 }
            }
        }

        // Secondary text
        Item {
            id: secondaryTextContainer
            anchors.horizontalCenter: parent.horizontalCenter
            y: 280
            width: secondaryText.width
            height: secondaryText.height
            opacity: 0

            property real breatheAmount: 1.0

            Behavior on opacity {
                NumberAnimation {
                    duration: 600
                    easing.type: Easing.OutCubic
                }
            }

            SequentialAnimation on breatheAmount {
                running: true
                loops: Animation.Infinite
                PauseAnimation { duration: 500 }
                NumberAnimation { from: 0.95; to: 1.0; duration: 2000; easing.type: Easing.InOutQuad }
                NumberAnimation { from: 1.0; to: 0.95; duration: 2000; easing.type: Easing.InOutQuad }
            }

            Text {
                id: secondaryText
                anchors.centerIn: parent
                text: root.secondaryText
                color: root.splashColor
                font.family: root.splashFont
                font.pixelSize: root.subtitleSize
                opacity: secondaryTextContainer.breatheAmount

                layer.enabled: root.useGlow
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowBlur: root.glowIntensity / 40.0
                    shadowOpacity: 0.6
                    shadowColor: root.splashGlow
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                }
            }
        }
    }

    // Fade out animation
    NumberAnimation on opacity {
        id: fadeOut
        running: false
        from: 1.0
        to: 0.0
        duration: root.fadeOutDuration
        easing.type: Easing.InOutQuad
        onFinished: root.finished()
    }

    // Primary text animation timer
    Timer {
        id: animTimer
        interval: root.charDuration
        repeat: true
        running: root.themeReady

        onTriggered: {
            if (root.stage === 0) {
                if (root.animText.length < root.primaryText.length) {
                    root.animText = root.primaryText.substring(0, root.animText.length + 1)
                } else {
                    root.stage = 1
                    animTimer.stop()
                    secondaryTimer.start()
                }
            }
        }
    }

    // Show secondary text
    Timer {
        id: secondaryTimer
        interval: 200
        onTriggered: {
            secondaryTextContainer.opacity = 1.0
            holdTimer.start()
        }
    }

    // Hold before fade
    Timer {
        id: holdTimer
        interval: root.holdDuration
        onTriggered: fadeOut.start()
    }
}
