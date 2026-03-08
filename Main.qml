import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Effects
import QtCore
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
    color:"#000000"

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

    property QtObject appPrefs: QtObject {
        property var settings: Qt.createQmlObject('import QtCore; Settings { }', mainWindow)
        property string recentAppsData: settings ? settings.value("recentAppsData", "") : ""

        function save(data) {
            if (settings) {
                settings.setValue("recentAppsData", data)
                recentAppsData = data
            }
        }
    }

    property var recentApps: []
    property string currentScreen: "home"

    property var appActivity: ({
        "music": { active: false, lastUsed: 0 },
        "spotify": { active: false, lastUsed: 0 },
        "radio": { active: false, lastUsed: 0 },
        "podcasts": { active: false, lastUsed: 0 },
        "maps": { active: false, lastUsed: 0 },
        "phone": { active: false, lastUsed: 0 },
        "camera": { active: false, lastUsed: 0 }
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

    // PicovoiceManager Connections - Unified Voice Pipeline
    Connections {
        target: picovoiceManager

        function onWakeWordDetected(keyword) {
            console.log("Wake word detected via PicovoiceManager:", keyword, "- Activating Claude AI")

            // CRITICAL: Stop any ongoing TTS playback immediately to prevent audio conflicts
            if (googleTTS.isSpeaking) {
                console.log("Stopping TTS playback before processing new wake word")
                googleTTS.stop()
            }

            // Cancel any pending hide timer
            hideClaudeTimer.stop()

            // Activate Claude indicator (same as long-press home button)
            claudeIndicatorLoader.active = true

            // Wait for loader to complete before showing
            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.show()
                claudeIndicatorLoader.item.setState("listening")
            }

            // Play a short "ready" voice prompt to indicate we're listening
            // Use a random phrase for personality variety
            var readyPhrases = ["Yes?", "What's up?", "I'm here", "Go ahead", "Listening"]
            var randomIndex = Math.floor(Math.random() * readyPhrases.length)
            var readyPhrase = readyPhrases[randomIndex]
            console.log("Playing ready prompt:", readyPhrase)
            googleTTS.speak(readyPhrase)

            // Note: PicovoiceManager is now in WaitingForReadyPrompt state
            // It will transition to listening mode when onReadyPromptFinished() is called
            // This is triggered by googleTTS.speechFinished signal (see Connections below)
        }

        function onIntentDetected(intent, slots) {
            console.log("Intent detected:", intent, "Slots:", JSON.stringify(slots))

            // Quick command recognized - hide indicator after brief display
            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.setState("processing")
            }

            // Intent is handled by VoiceCommandHandler (already connected in main.cpp)
            // Auto-hide after 2 seconds for quick commands
            hideClaudeTimer.start()
        }

        function onTranscriptionReady(text) {
            console.log("Speech transcription from Leopard:", text)

            // Update Claude indicator to show we're processing with AI
            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.setState("processing")
            }

            // Transcription is sent to Claude AI (already connected in main.cpp)
            // Claude will respond, which is handled by claudeClient connections below
        }

        function onError(message) {
            console.log("PicovoiceManager error:", message)
            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.hide()
            }
            showNotification("Voice Error: " + message, "error")
        }
    }

    // Google TTS Connections - for ready prompt completion
    Connections {
        target: googleTTS

        function onSpeechFinished() {
            // Notify PicovoiceManager that the ready prompt has finished
            // so it can start listening for the command
            console.log("TTS speech finished, notifying PicovoiceManager")
            picovoiceManager.onReadyPromptFinished()
        }
    }

    // Claude Client Connections
    Connections {
        target: claudeClient

        function onProcessingChanged() {
            if (claudeIndicatorLoader.item) {
                if (claudeClient.isProcessing) {
                    claudeIndicatorLoader.item.setState("processing")
                } else {
                    // If not processing anymore, hide after a delay
                    hideClaudeTimer.start()
                }
            }
        }

        function onResponseReceived(response, toolCalls) {
            console.log("Claude response:", response)
            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.setState("speaking")
            }

            // Process voice commands from response
            voiceCommandHandler.processClaudeResponse(response)

            // Speak Claude's response using Google TTS
            googleTTS.speak(response)

            // Hide indicator after speaking (3 seconds for now, will be based on speech length later)
            hideClaudeTimer.start()
        }

        function onError(message) {
            console.log("Claude error:", message)
            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.hide()
            }
            showNotification("Claude Error: " + message, "error")
        }
    }

    // Voice Command Handler Connections
    Connections {
        target: voiceCommandHandler

        function onConfirmationRequested(action, details) {
            console.log("Voice command confirmation requested:", action, details)
            voiceConfirmationDialog.action = action
            voiceConfirmationDialog.details = details
            voiceConfirmationDialog.visible = true
        }

        function onCommandExecuted(action, details) {
            console.log("Voice command executed:", action, details)
        }

        function onCommandFailed(action, error) {
            console.log("Voice command failed:", action, error)
            showNotification("Command failed: " + error, "error")
        }
    }

    // Timer to hide Claude indicator after response
    Timer {
        id: hideClaudeTimer
        interval: 3000
        repeat: false
        onTriggered: {
            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.hide()
            }
        }
    }

    // ========== AUDIO SOURCE MANAGEMENT ==========
    property string activeAudioSource: "none"

    function setAudioSource(source) {
        if (activeAudioSource !== source) {
            console.log("Audio source switching:", activeAudioSource, "→", source)

            if (source !== "phone") mediaController.pause()

            activeAudioSource = source
        }
    }

    // ========== SCREEN NAVIGATION ==========
    function showScreen(key) {
        console.log("Showing screen:", key)
        currentScreen = key

        if (appActivity.hasOwnProperty(key)) {
            appActivity[key].active = true
            appActivity[key].lastUsed = Date.now()
        }

        updateRecentApps(key)

        homeLoader.active = (key === "home")
        settingsLoader.active = (key === "settings")
        musicLoader.active = (key === "music")
        spotifyLoader.active = (key === "spotify")
        radioLoader.active = (key === "radio")
        podcastsLoader.active = (key === "podcasts")
        mapsLoader.active = (key === "maps")
        phoneLoader.active = (key === "phone")
        messagesLoader.active = (key === "messages")
        contactsLoader.active = (key === "contacts")
        cameraLoader.active = (key === "camera")
        climateLoader.active = (key === "climate")
        vehicleLoader.active = (key === "vehicle")
        weatherLoader.active = (key === "weather")
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

    // Auto-connect to phone on startup
    Timer {
        id: autoConnectTimer
        interval: 2000
        running: true
        repeat: false
        onTriggered: {
            var connectedAddress = bluetoothManager.getFirstConnectedDeviceAddress()

            if (connectedAddress !== "") {
                console.log("Device already connected, setting up services:", connectedAddress)
                setupDeviceServices(connectedAddress)
            } else if (appSettings.autoConnectBluetooth) {
                // Auto-connect is enabled, check for paired devices
                var pairedAddress = bluetoothManager.getFirstPairedDeviceAddress()
                if (pairedAddress !== "") {
                    console.log("Auto-connect enabled: connecting to paired device:", pairedAddress)
                    bluetoothManager.connectToDevice(pairedAddress)
                    // Wait for deviceConnected signal (handled below)
                } else {
                    console.log("Auto-connect enabled but no paired device found")
                }
            } else {
                console.log("Auto-connect disabled in settings")
            }
        }
    }

    // Handle Bluetooth events
    Connections {
        target: bluetoothManager

        function onDeviceConnected(address) {
            console.log("Device connected successfully:", address)
            var deviceName = bluetoothManager.getDeviceName(address)
            showNotification("Connected to " + deviceName, "success")
            setupDeviceServices(address)
        }

        function onDeviceDisconnected(address) {
            var deviceName = bluetoothManager.getDeviceName(address)
            showNotification(deviceName + " disconnected", "info")
        }

        function onDevicePaired(address) {
            var deviceName = bluetoothManager.getDeviceName(address)
            showNotification(deviceName + " paired successfully", "success")
        }

        function onError(message) {
            showNotification(message, "error")
        }

        function onActiveCallChanged() {
            console.log("Call state changed - hasActiveCall:", bluetoothManager.hasActiveCall)

            if (bluetoothManager.hasActiveCall) {
                // Call started - pause music
                console.log("Call started - pausing music")
                mediaController.pause()
            } else {
                // Call ended - resume music (only if we were on a music source)
                console.log("Call ended - resuming music")
                if (activeAudioSource === "music" || activeAudioSource === "spotify") {
                    mediaController.play()
                }
            }
        }
    }

    // Setup device services (MediaController, NotificationManager, etc.)
    function setupDeviceServices(address) {
        console.log("Setting up device services for:", address)
        mediaController.connectToDevice(address)
        notificationManager.connectToDevice(address, "ios")
        voiceAssistant.connectToPhone(address)
    }

    // Show system notification
    function showNotification(message, type) {
        var notification = {
            appName: "Bluetooth",
            title: type === "success" ? "✓" : type === "error" ? "✕" : "ⓘ",
            message: message,
            priority: type === "error" ? 3 : 1
        }
        notificationBanner.showNotification(notification)
    }

    // Timer to wait for voice connection
    Timer {
        id: connectionWaitTimer
        interval: 1500
        running: false
        repeat: false
        onTriggered: {
            if (voiceAssistant.isConnected) {
                voiceControlLoader.active = true
                voiceAssistant.activateAssistant()
            } else {
                console.log("Voice assistant connection failed")
            }
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
                theme.load(theme.name === "Cyberpunk" ? "RetroVFD" : "Cyberpunk")
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
                    console.log("Claude AI activated via home button long press")

                    // Activate Claude indicator
                    claudeIndicatorLoader.active = true

                    // Wait for loader to complete before showing
                    if (claudeIndicatorLoader.item) {
                        claudeIndicatorLoader.item.show()
                        claudeIndicatorLoader.item.setState("listening")
                    }
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
                        claudeIndicatorLoader.active = false
                        claudeClient.cancelRequest()
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

            // Voice Command Confirmation Dialog
            Rectangle {
                id: voiceConfirmationDialog
                anchors.centerIn: parent
                width: 500
                height: 200
                radius: 12
                color: Qt.rgba(0, 0, 0, 0.95)
                border.color: theme.palette.primary
                border.width: 2
                visible: false
                z: 1002  // Above everything else

                property string action: ""
                property string details: ""

                Column {
                    anchors.centerIn: parent
                    spacing: 20

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: voiceConfirmationDialog.details
                        color: theme.palette.text
                        font.pixelSize: 18
                        font.family: theme.typography.fontFamily
                        width: 450
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 20

                        Rectangle {
                            width: 120
                            height: 50
                            radius: 8
                            color: "transparent"
                            border.color: theme.palette.primary
                            border.width: 2

                            Text {
                                anchors.centerIn: parent
                                text: "Yes, Send"
                                color: theme.palette.primary
                                font.pixelSize: 16
                                font.family: theme.typography.fontFamily
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    voiceCommandHandler.confirmAction()
                                    voiceConfirmationDialog.visible = false
                                }
                            }
                        }

                        Rectangle {
                            width: 120
                            height: 50
                            radius: 8
                            color: "transparent"
                            border.color: theme.palette.accent
                            border.width: 2

                            Text {
                                anchors.centerIn: parent
                                text: "Cancel"
                                color: theme.palette.accent
                                font.pixelSize: 16
                                font.family: theme.typography.fontFamily
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    voiceCommandHandler.cancelAction()
                                    voiceConfirmationDialog.visible = false
                                }
                            }
                        }
                    }
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

            Item {
    id: screenContainer
    anchors {
        left: navBar.right
        leftMargin: 0     // ← Reset to 0
        right: parent.right
        rightMargin: 0    // ← Reset to 0
        top: statusBar.bottom
        topMargin: 0      // ← Reset to 0, statusBar.bottom already positions it correctly
        bottom: parent.bottom
        bottomMargin: 100   // ← Space for mini player
    }

                Loader {
                    id: homeLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "home"
                    active: true
                    asynchronous: true
                    source: "Ui/screens/Home.qml"
                    onLoaded: {
                        item.theme = theme
                        if (item.appSelected) {
                            item.appSelected.connect(showScreen)
                        }
                    }
                }

                Loader {
                    id: settingsLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "settings"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Settings.qml"
                    onLoaded: {
                        item.theme = theme
                        item.appSettings = appSettings
                    }
                }

                Loader {
                    id: musicLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "music"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Music.qml"
                    onLoaded: {
                        item.theme = theme
                        if (item.messageFromJs) {
                            item.messageFromJs.connect(function(msg) {
                                console.log("Music screen message:", msg)
                            })
                        }
                    }
                }

                Loader {
                    id: spotifyLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "spotify"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Spotify.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: radioLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "radio"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Radio.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: podcastsLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "podcasts"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Podcasts.qml"
                    onLoaded: { item.theme = theme }
                }

                 Loader {
                    id: mapsLoader
                    anchors.fill: parent
                    // No margin adjustments - use natural 12px from Page.qml
                    visible: mainWindow.currentScreen === "maps"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Maps.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: phoneLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "phone"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Phone.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: messagesLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "messages"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Messages.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: contactsLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "contacts"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Contacts.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: cameraLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "camera"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Camera.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: climateLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "climate"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Climate.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: vehicleLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "vehicle"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Vehicle.qml"
                    onLoaded: { item.theme = theme }
                }

                Loader {
                    id: weatherLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "weather"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Weather.qml"
                    onLoaded: { item.theme = theme }
                }
            }

            // Mini Player - Bottom Bar
            MiniPlayer {
                id: miniPlayer
                theme: rootPage.theme
                anchors {
                    left: navBar.right
                    right: parent.right
                    bottom: parent.bottom
                }
                z: 997  // Above screens, below overlays
            }
        }
    }
}
