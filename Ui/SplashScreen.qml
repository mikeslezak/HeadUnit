import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import QtMultimedia

Rectangle {
    id: root
    anchors.fill: parent
    required property var theme

    signal finished()

    property int stage: 0
    property string animText: ""
    property bool themeReady: false

    // Video splash support — check if theme has a splash video
    readonly property string splashVideoFile: theme?.splash?.video ?? ""
    readonly property bool hasVideo: splashVideoFile.length > 0

    // Resolve video path: theme videos live on filesystem (too large for qrc)
    readonly property url videoUrl: {
        if (!hasVideo || !theme?.name) return ""
        // projectDir is set in main.cpp as file:///path/to/HeadUnit/
        return projectDir + "themes/" + theme.name + "/" + splashVideoFile
    }

    onThemeReadyChanged: {
        if (themeReady) {
            if (root.hasVideo) {
                console.log("Splash: themeReady, using video mode:", root.videoUrl)
                // Delay video start slightly to let the main thread unblock from theme loading
                videoStartTimer.start()
            } else if (root.animStyle === "fadeIn") {
                console.log("Splash: themeReady, using fadeIn mode")
                root.animText = root.splashPrimaryText
                fadeInSequence.start()
            } else if (!animTimer.running) {
                console.log("Splash: themeReady changed to true, starting animTimer")
                animTimer.start()
            }
        }
    }

    // Theme tokens with comprehensive fallbacks
    readonly property color splashBg: theme?.splash?.bg ?? "#000000"
    readonly property color splashColor: theme?.splash?.color ?? "#00d9d9"
    readonly property color splashGlow: theme?.splash?.glow ?? "#00d9d966"
    readonly property string splashFont: theme?.typography?.fontFamily ?? "Noto Sans"

    readonly property string logoAnim: theme?.splash?.logoAnim ?? "breathe"
    readonly property string splashPrimaryText: theme?.splash?.primaryText ?? "CHEVROLET"
    readonly property string splashSecondaryText: theme?.splash?.secondaryText ?? "DELCO ELECTRONICS"

    // Sizing
    readonly property int logoSize: Number(theme?.splash?.logoSize ?? 250)
    readonly property int titleSize: Number(theme?.splash?.titleSize ?? 42)
    readonly property int subtitleSize: Number(theme?.splash?.subtitleSize ?? 18)

    // Timing
    readonly property int charDuration: Number(theme?.splash?.charDuration ?? 180)
    readonly property int holdDuration: Number(theme?.splash?.holdDuration ?? 12000)
    readonly property int fadeOutDuration: Number(theme?.splash?.fadeOutDuration ?? 3500)

    // FadeIn timing
    readonly property int fadeInLogoDuration: Number(theme?.splash?.fadeInLogoDuration ?? 1500)
    readonly property int fadeInTextDelay: Number(theme?.splash?.fadeInTextDelay ?? 800)
    readonly property int fadeInTextDuration: Number(theme?.splash?.fadeInTextDuration ?? 1200)
    readonly property int fadeInSubtitleDelay: Number(theme?.splash?.fadeInSubtitleDelay ?? 600)
    readonly property int fadeInSubtitleDuration: Number(theme?.splash?.fadeInSubtitleDuration ?? 1000)
    readonly property int fadeInLineDelay: Number(theme?.splash?.fadeInLineDelay ?? 400)
    readonly property int fadeInLineDuration: Number(theme?.splash?.fadeInLineDuration ?? 800)

    // Effects
    readonly property bool showScanlines: (theme?.splash?.scanlines ?? true)
    readonly property real scanlineOpacity: Number(theme?.splash?.scanlineOpacity ?? 0.15)
    readonly property int scanlineSpacing: Number(theme?.splash?.scanlineSpacing ?? 3)
    readonly property bool useGlow: (theme?.splash?.useGlow ?? true)
    readonly property real glowIntensity: Number(theme?.splash?.glowIntensity ?? 8)
    readonly property string logoColorMode: theme?.splash?.logoColorMode ?? "colorize"
    readonly property string animStyle: theme?.splash?.animStyle ?? "typewriter"
    readonly property real letterSpacing: Number(theme?.splash?.letterSpacing ?? 0)
    readonly property real subtitleLetterSpacing: Number(theme?.splash?.subtitleLetterSpacing ?? 0)
    readonly property bool useVignette: (theme?.splash?.useVignette ?? false)
    readonly property bool useLine: (theme?.splash?.useLine ?? false)
    readonly property color lineColor: theme?.splash?.lineColor ?? "#333333"

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
        id: videoStartTimer
        interval: 50
        onTriggered: {
            console.log("Splash: Starting video playback")
            videoPlayer.source = root.videoUrl
            videoPlayer.play()
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

    // ══════════════════════════════════════════════════
    // ── VIDEO SPLASH ──
    // ══════════════════════════════════════════════════

    MediaPlayer {
        id: videoPlayer
        videoOutput: videoOutput

        onMediaStatusChanged: {
            if (mediaStatus === MediaPlayer.EndOfMedia) {
                console.log("Splash: Video playback finished")
                root.finished()
            }
        }

        onErrorOccurred: function(error, errorString) {
            console.warn("Splash: Video error:", errorString, "- falling back to QML animation")
            // Fall back to QML animation if video fails
            if (root.animStyle === "fadeIn") {
                root.animText = root.splashPrimaryText
                fadeInSequence.start()
            } else {
                animTimer.start()
            }
        }
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        visible: root.hasVideo && videoPlayer.playbackState !== MediaPlayer.StoppedState
    }

    // ══════════════════════════════════════════════════
    // ── QML FALLBACK ANIMATION ──
    // ══════════════════════════════════════════════════

    // Only visible when NOT using video
    Item {
        id: qmlSplashContent
        anchors.fill: parent
        visible: !root.hasVideo || videoPlayer.error !== MediaPlayer.NoError

        // ── FadeIn sequence animation ──
        SequentialAnimation {
            id: fadeInSequence

            NumberAnimation {
                target: logoContainer
                property: "opacity"
                from: 0; to: 1
                duration: root.fadeInLogoDuration
                easing.type: Easing.OutCubic
            }

            PauseAnimation { duration: root.fadeInLineDelay }

            ParallelAnimation {
                NumberAnimation {
                    target: decorativeLine
                    property: "opacity"
                    from: 0; to: 0.6
                    duration: root.fadeInLineDuration
                    easing.type: Easing.OutCubic
                }

                SequentialAnimation {
                    PauseAnimation { duration: root.fadeInTextDelay }
                    NumberAnimation {
                        target: primaryTextContainer
                        property: "fadeInOpacity"
                        from: 0; to: 1
                        duration: root.fadeInTextDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }

            PauseAnimation { duration: root.fadeInSubtitleDelay }

            NumberAnimation {
                target: secondaryTextContainer
                property: "opacity"
                from: 0; to: 1
                duration: root.fadeInSubtitleDuration
                easing.type: Easing.OutCubic
            }

            PauseAnimation { duration: root.holdDuration }

            ScriptAction {
                script: {
                    console.log("Splash: FadeIn hold complete, starting fade out")
                    scanlinePulse.stop()
                    scanlineFadeOut.start()
                    fadeOut.start()
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

                var step = Math.max(root.scanlineSpacing, 2)
                for (var y = 0; y < height; y += step) {
                    ctx.beginPath()
                    ctx.moveTo(0, y)
                    ctx.lineTo(width, y)
                    ctx.stroke()
                }
            }

            NumberAnimation on opacity {
                id: scanlinePulse
                from: scanlineOpacity * 0.8
                to: scanlineOpacity * 1.2
                duration: 2000
                loops: Animation.Infinite
                easing.type: Easing.InOutSine
            }

            NumberAnimation {
                id: scanlineFadeOut
                target: scanlinesCanvas
                property: "opacity"
                to: 0.0
                duration: root.fadeOutDuration
                easing.type: Easing.InOutQuad
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

            // Logo
            Item {
                id: logoContainer
                anchors.horizontalCenter: parent.horizontalCenter
                y: 40
                width: root.logoSize
                height: root.logoSize * 0.35
                opacity: 0

                property real animValue: 1.0

                Behavior on opacity {
                    enabled: root.animStyle !== "fadeIn"
                    NumberAnimation {
                        duration: 800
                        easing.type: Easing.OutCubic
                    }
                }

                Component.onCompleted: {
                    if (root.animStyle !== "fadeIn") {
                        Qt.callLater(function() {
                            logoContainer.opacity = 1
                        })
                    }
                }

                SequentialAnimation on animValue {
                    running: root.logoAnim === "breathe"
                    loops: Animation.Infinite
                    PauseAnimation { duration: 500 }
                    NumberAnimation { from: 0.95; to: 1.0; duration: 2000; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 1.0; to: 0.95; duration: 2000; easing.type: Easing.InOutQuad }
                }

                SequentialAnimation on animValue {
                    running: root.logoAnim === "pulse"
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 1.1; duration: 300; easing.type: Easing.OutQuad }
                    NumberAnimation { from: 1.1; to: 1.0; duration: 300; easing.type: Easing.InQuad }
                    PauseAnimation { duration: 1400 }
                }

                SequentialAnimation on animValue {
                    running: root.logoAnim === "glow"
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.7; to: 1.0; duration: 1500; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 1.0; to: 0.7; duration: 1500; easing.type: Easing.InOutSine }
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
                            source = Qt.resolvedUrl("qrc:/HeadUnit/themes/" + theme.name + "/" + String(theme.splash.logo))
                            console.log("Splash: Logo source set to:", source)
                        }
                    }

                    layer.enabled: root.logoColorMode === "colorize"
                    layer.smooth: false
                    layer.effect: ColorOverlay {
                        color: root.splashColor
                    }
                }
            }

            // Primary text
            Item {
                id: primaryTextContainer
                anchors.horizontalCenter: parent.horizontalCenter
                y: 200
                width: primaryTextLabel.width
                height: primaryTextLabel.height

                property real fadeInOpacity: 0
                opacity: root.animStyle === "fadeIn" ? fadeInOpacity : (root.animText.length > 0 ? 1.0 : 0)

                property real breatheAmount: 1.0

                SequentialAnimation on breatheAmount {
                    running: root.animStyle !== "fadeIn"
                    loops: Animation.Infinite
                    PauseAnimation { duration: 500 }
                    NumberAnimation { from: 0.95; to: 1.0; duration: 2000; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 1.0; to: 0.95; duration: 2000; easing.type: Easing.InOutQuad }
                }

                Text {
                    id: primaryTextLabel
                    anchors.centerIn: parent
                    text: root.animText
                    color: root.splashColor
                    font.family: root.splashFont
                    font.pixelSize: root.titleSize
                    font.weight: Font.Bold
                    font.letterSpacing: root.letterSpacing
                    opacity: root.animStyle === "fadeIn" ? 1.0 : primaryTextContainer.breatheAmount

                    layer.enabled: root.useGlow
                    layer.effect: Glow {
                        color: root.splashGlow
                        spread: 0.5
                        radius: root.glowIntensity
                        samples: 17
                    }
                }
            }

            // Blinking cursor (typewriter mode only)
            Text {
                visible: root.animStyle !== "fadeIn" && root.animText.length > 0 && root.animText.length < root.splashPrimaryText.length
                anchors.horizontalCenter: parent.horizontalCenter
                x: primaryTextContainer.x + primaryTextLabel.width + 4
                y: primaryTextContainer.y
                text: "_"
                color: root.splashColor
                font.family: root.splashFont
                font.pixelSize: root.titleSize
                font.letterSpacing: root.letterSpacing

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
                width: secondaryTextLabel.width
                height: secondaryTextLabel.height
                opacity: 0

                property real breatheAmount: 1.0

                Behavior on opacity {
                    enabled: root.animStyle !== "fadeIn"
                    NumberAnimation { duration: 600; easing.type: Easing.OutCubic }
                }

                SequentialAnimation on breatheAmount {
                    running: root.animStyle !== "fadeIn"
                    loops: Animation.Infinite
                    PauseAnimation { duration: 500 }
                    NumberAnimation { from: 0.95; to: 1.0; duration: 2000; easing.type: Easing.InOutQuad }
                    NumberAnimation { from: 1.0; to: 0.95; duration: 2000; easing.type: Easing.InOutQuad }
                }

                Text {
                    id: secondaryTextLabel
                    anchors.centerIn: parent
                    text: root.splashSecondaryText
                    color: root.splashColor
                    font.family: root.splashFont
                    font.pixelSize: root.subtitleSize
                    font.letterSpacing: root.subtitleLetterSpacing
                    opacity: root.animStyle === "fadeIn" ? 1.0 : secondaryTextContainer.breatheAmount

                    layer.enabled: root.useGlow
                    layer.effect: Glow {
                        color: root.splashGlow
                        spread: 0.5
                        radius: root.glowIntensity
                        samples: 17
                    }
                }
            }

            // Decorative line
            Rectangle {
                id: decorativeLine
                visible: root.useLine
                anchors.horizontalCenter: parent.horizontalCenter
                y: 180
                width: 120
                height: 1
                color: root.lineColor
                opacity: root.animStyle === "fadeIn" ? 0 : (root.animText.length > 0 ? 0.8 : 0)

                Behavior on opacity {
                    enabled: root.animStyle !== "fadeIn"
                    NumberAnimation { duration: 600; easing.type: Easing.OutCubic }
                }
            }
        }

        // Vignette overlay
        RadialGradient {
            visible: root.useVignette
            anchors.fill: parent
            z: 50

            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.55; color: "transparent" }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.7) }
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
    }

    // ── Typewriter mode timers ──

    Timer {
        id: animTimer
        interval: root.charDuration
        repeat: true
        running: false

        onRunningChanged: {
            console.log("Splash: animTimer running =", running)
        }

        onTriggered: {
            if (root.stage === 0) {
                if (root.animText.length < root.splashPrimaryText.length) {
                    root.animText = root.splashPrimaryText.substring(0, root.animText.length + 1)
                } else {
                    console.log("Splash: Text animation complete, starting secondary timer")
                    root.stage = 1
                    animTimer.stop()
                    secondaryTimer.start()
                }
            }
        }
    }

    Timer {
        id: secondaryTimer
        interval: 200
        onTriggered: {
            console.log("Splash: Secondary timer triggered, showing subtitle and starting hold")
            secondaryTextContainer.opacity = 1.0
            holdTimer.start()
        }
    }

    Timer {
        id: holdTimer
        interval: root.holdDuration
        onTriggered: {
            console.log("Splash: Hold timer triggered, starting fade out")
            scanlinePulse.stop()
            scanlineFadeOut.start()
            fadeOut.start()
        }
    }
}
