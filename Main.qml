import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Effects
import QtCore
import HeadUnit
import "Ui"

Window {
    id: mainWindow
    width: 1560
    height: 720
    minimumWidth: 1560
    maximumWidth: 1560
    minimumHeight: 720
    maximumHeight: 720
    visible: true
    title: "HeadUnit"
    visibility: Window.FullScreen
    color: ThemeValues.bgCol

    Theme { id: theme }

    // Shared app settings (accessible from all screens)
    Settings {
        id: appSettings
        property bool autoReadMessages: true
        property int voiceVolume: 80
        property bool use24HourFormat: false
        property bool autoConnectBluetooth: true
        property string lastBluetoothDevice: ""
        property string ttsVoice: "en-US-Neural2-F"
        property double ttsSpeakingRate: 1.0
        property double ttsPitch: 0.0
    }

    Settings {
        id: appPrefsSettings
        property string recentAppsData: ""
        property string lastScreen: "home"
    }

    property QtObject appPrefs: QtObject {
        property string recentAppsData: appPrefsSettings.recentAppsData

        function save(data) {
            appPrefsSettings.recentAppsData = data
            recentAppsData = data
        }
    }

    property var recentApps: []
    property string currentScreen: "home"

    // Mapbox access token - passed from C++ via context property
    readonly property string mapboxToken: typeof mapboxAccessToken !== 'undefined' ? mapboxAccessToken : ""

    property var appActivity: ({
        "music": { active: false, lastUsed: 0 },
        "phone": { active: false, lastUsed: 0 },
        "weather": { active: false, lastUsed: 0 }
    })

    readonly property int inactivityTimeout: 300000
    readonly property int topBufferHeight: 70  // Space reserved for status bar

    // Notification Manager Connections
    Connections {
        target: notificationManager

        function onNotificationReceived(notification) {
            console.log("New notification:", notification.title)
            // Show banner popup instead of panel
            notificationBanner.showNotification(notification)
        }

        function onNotificationDismissed(notificationId) {
            console.log("Notification dismissed:", notificationId)
        }
    }

    // Voice Assistant Connections
    Connections {
        target: voiceAssistant

        function onListeningChanged() {
            console.log("Voice listening:", voiceAssistant.isListening)
            if (voiceAssistant.isListening) {
                voiceControlLoader.active = true
            }
        }

        function onCommandRecognized(command) {
            console.log("Voice command:", command)
            voiceControlLoader.active = false
        }
    }

    // ========== AUDIO SOURCE MANAGEMENT ==========
    property string activeAudioSource: "none"

    function setAudioSource(source) {
        if (activeAudioSource !== source) {
            console.log("Audio source switching:", activeAudioSource, "→", source)

            // Pause outgoing source
            if (activeAudioSource === "phone" || activeAudioSource === "music") mediaController.pause()
            if (activeAudioSource === "tidal" && tidalClient) tidalClient.pause()
            if (activeAudioSource === "spotify" && spotifyClient) spotifyClient.pause()

            activeAudioSource = source
        }
    }

    // Auto-switch source when Tidal starts playing
    Connections {
        target: tidalClient
        function onPlayStateChanged() {
            if (tidalClient.isPlaying) {
                setAudioSource("tidal")
            }
        }
    }

    // Auto-switch source when Spotify starts playing
    Connections {
        target: spotifyClient
        function onPlayStateChanged() {
            if (spotifyClient.isPlaying) {
                setAudioSource("spotify")
            }
        }
    }

    // Auto-switch source when Bluetooth media starts playing
    Connections {
        target: mediaController
        function onPlayStateChanged() {
            if (mediaController.isPlaying) {
                setAudioSource("music")
            }
        }
    }

    // ========== SCREEN NAVIGATION ==========
    function showScreen(key) {
        console.log("Showing screen:", key)
        currentScreen = key
        appPrefsSettings.lastScreen = key

        if (appActivity.hasOwnProperty(key)) {
            appActivity[key].active = true
            appActivity[key].lastUsed = Date.now()
        }

        updateRecentApps(key)
        screenContainer.show(key)
        navBar.activeKey = key
    }

    function updateRecentApps(appKey) {
        // Don't add home to recent apps
        if (appKey === "home") return

        var recent = recentApps.slice()
        var idx = recent.indexOf(appKey)

        if (idx !== -1) {
            recent.splice(idx, 1)
        }

        recent.unshift(appKey)

        if (recent.length > 3) {
            recent = recent.slice(0, 3)
        }

        recentApps = recent
        appPrefs.save(JSON.stringify(recent))
    }

    // ========== INACTIVITY TRACKING ==========
    Timer {
        id: inactivityTimer
        interval: inactivityTimeout
        running: false
        repeat: false
        onTriggered: {
            console.log("Inactivity timeout - marking apps inactive")
            for (var key in appActivity) {
                appActivity[key].active = false
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        onPressed: {
            inactivityTimer.restart()
            mouse.accepted = false
        }
    }

    // Bluetooth connection management (auto-connect, events, services)
    BluetoothHandler {
        id: bluetoothHandler
        theme: theme
        appSettings: appSettings
        activeAudioSource: mainWindow.activeAudioSource
        onNotificationRequested: function(notification) {
            notificationBanner.showNotification(notification)
        }
    }

    Component.onCompleted: {
        try {
            var saved = appPrefs.recentAppsData
            if (saved) {
                recentApps = JSON.parse(saved)
                console.log("Loaded recent apps:", recentApps)
            }
        } catch (e) {
            console.log("Failed to parse recent apps:", e)
            recentApps = []
        }

        if (Qt.platform.os === "windows") {
            notificationManager.generateMockNotifications()
        }
    }

    // Splash Screen - shows on boot
    SplashScreen {
        id: splash
        anchors.fill: parent
        z: 10000
        visible: true
        theme: theme

        onFinished: {
            splash.visible = false
            stage.focus = true

            // Restore last screen
            var last = appPrefsSettings.lastScreen
            if (last && last !== "home") {
                showScreen(last)
            }
        }
    }

    FocusScope {
        id: stage
        anchors.fill: parent
        focus: false

        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Home || event.key === Qt.Key_H) {
                showScreen("home")
                event.accepted = true
            }
            else if (event.key === Qt.Key_M) {
                showScreen("music")
                event.accepted = true
            }
            else if (event.key === Qt.Key_S) {
                showScreen("settings")
                event.accepted = true
            }
            else if (event.key === Qt.Key_T) {
                var themes = ["Cyberpunk", "RetroVFD", "Monochrome"]
                var idx = (themes.indexOf(theme.name) + 1) % themes.length
                theme.load(themes[idx])
                event.accepted = true
            }
        }

        Page {
            id: rootPage
            anchors.fill: parent
            theme: theme

            NavBar {
                id: navBar
                theme: theme
                recentApps: mainWindow.recentApps
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                onSelected: function (key) {
                    showScreen(key)
                }

                onHomeLongPressed: {
                    voicePipeline.activate()
                }
            }

            // Notification Panel Overlay
            Loader {
                id: notificationPanelLoader
                anchors.fill: parent
                active: false
                asynchronous: true
                source: "Ui/NotificationPanel.qml"
                z: 1000

                onLoaded: {
                    item.theme = theme
                    item.closeRequested.connect(function() {
                        notificationPanelLoader.active = false
                    })
                    console.log("NotificationPanel loaded")
                }
            }

            // Voice Control Overlay (for phone-based assistant)
            Loader {
                id: voiceControlLoader
                anchors.fill: parent
                active: false
                asynchronous: true
                source: "Ui/VoiceControl.qml"
                z: 999

                onLoaded: {
                    item.theme = theme
                    item.closeRequested.connect(function() {
                        voiceControlLoader.active = false
                        voiceAssistant.stopListening()
                    })
                    console.log("VoiceControl loaded")
                }
            }

            // Claude AI Indicator Overlay
            Loader {
                id: claudeIndicatorLoader
                anchors.fill: parent
                active: false
                asynchronous: false  // Synchronous so theme is set before show()
                source: "Ui/ClaudeIndicator.qml"
                z: 1001  // Above voice control

                onLoaded: {
                    item.theme = theme
                    item.closeRequested.connect(function() {
                        console.log("ClaudeIndicator: closeRequested fired — canceling voice pipeline")
                        claudeIndicatorLoader.active = false
                        claudeClient.cancelRequest()
                        googleTTS.stop()
                        picovoiceManager.cancelAndReset()
                        voicePipeline.cancelInteraction()
                    })
                    console.log("ClaudeIndicator loaded with theme:", theme.name)
                }
            }

            // Active Call Overlay
            Loader {
                id: activeCallOverlayLoader
                anchors.fill: parent
                active: true  // Always loaded, visibility controlled by bluetoothManager.hasActiveCall
                asynchronous: true
                source: "Ui/ActiveCallOverlay.qml"
                z: 1003  // Above everything except confirmation dialogs

                onLoaded: {
                    item.bluetoothManager = bluetoothManager
                    item.theme = theme
                    console.log("ActiveCallOverlay loaded")
                }
            }

            // Incoming Call Overlay
            Loader {
                id: incomingCallOverlayLoader
                anchors.fill: parent
                active: true  // Always loaded, visibility controlled by activeCallState
                asynchronous: true
                source: "Ui/IncomingCallOverlay.qml"
                z: 1004  // Above active call overlay

                onLoaded: {
                    item.bluetoothManager = bluetoothManager
                    item.theme = theme
                    console.log("IncomingCallOverlay loaded")
                }
            }

            // Voice Pipeline - all voice assistant signal wiring
            VoicePipeline {
                id: voicePipeline
                claudeIndicatorLoader: claudeIndicatorLoader
                voiceControlLoader: voiceControlLoader
                theme: theme
                screenContainer: screenContainer
                onNotificationRequested: function(message, type) {
                    var notification = {
                        appName: "Voice",
                        title: type === "success" ? "✓" : type === "error" ? "✕" : "ⓘ",
                        message: message,
                        priority: type === "error" ? 3 : 1
                    }
                    notificationBanner.showNotification(notification)
                }
                onNavigateToDestination: function(destination) {
                    showScreen("home")
                    screenContainer.navigateTo(destination)
                }
            }

            // Status Bar - occupies top buffer zone
            StatusBar {
                id: statusBar
                anchors.left: navBar.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 15
                height: 40
                theme: rootPage.theme
                z: 998
            }

            // CarPlay-Style Notification Banner
            NotificationBanner {
                id: notificationBanner
                anchors.fill: parent
                theme: rootPage.theme
                z: 9999

                onActionClicked: function(action) {
                    if (action === "view") {
                        notificationPanelLoader.active = true
                    }
                }
            }

            ScreenContainer {
                id: screenContainer
                theme: theme
                currentScreen: mainWindow.currentScreen
                mapboxToken: mainWindow.mapboxToken
                appSettings: appSettings
                anchors {
                    left: navBar.right
                    right: parent.right
                    top: statusBar.bottom
                    bottom: parent.bottom
                }
                onScreenSelected: function(key) {
                    showScreen(key)
                }
            }

            // Glance Panel — right-side overlay on home screen
            GlancePanel {
                id: glancePanel
                anchors.top: statusBar.bottom
                anchors.bottom: parent.bottom
                width: 280
                z: 10

                theme: rootPage.theme
                isHomeScreen: mainWindow.currentScreen === "home"
                audioSource: mainWindow.activeAudioSource

                navActive: screenContainer.navActive
                maneuver: screenContainer.navManeuver
                instruction: screenContainer.navInstruction
                stepDistance: screenContainer.navStepDistance
                routeDuration: screenContainer.navRouteDuration

                onNavTapped: showScreen("home")
                onMusicTapped: {
                    if (activeAudioSource === "tidal") showScreen("tidal")
                    else if (activeAudioSource === "spotify") showScreen("spotify")
                    else showScreen("music")
                }
            }
        }
    }
}
