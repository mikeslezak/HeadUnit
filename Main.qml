import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Effects
import "Ui"

Window {
    id: mainWindow
    width: 932
    height: 430
    minimumWidth: 932
    maximumWidth: 932
    minimumHeight: 430
    maximumHeight: 430
    visible: true
    title: "HeadUnit"
    visibility: Window.FullScreen
    color:"#000000"

    Theme { id: theme }

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
        "tidal": { active: false, lastUsed: 0 },
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

    // ========== AUDIO SOURCE MANAGEMENT ==========
    property string activeAudioSource: "none"

    function setAudioSource(source) {
        if (activeAudioSource !== source) {
            console.log("Audio source switching:", activeAudioSource, "→", source)

            if (source !== "phone") mediaController.pause()
            if (source !== "tidal") tidalController.pause()

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
        tidalLoader.active = (key === "tidal")
        spotifyLoader.active = (key === "spotify")
        radioLoader.active = (key === "radio")
        podcastsLoader.active = (key === "podcasts")
        mapsLoader.active = (key === "maps")
        phoneLoader.active = (key === "phone")
        cameraLoader.active = (key === "camera")
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
            console.log("Auto-connecting to iPhone...")
            notificationManager.connectToDevice("00:00:00:00:00:00", "ios")
            voiceAssistant.connectToPhone("00:00:00:00:00:00")
        }
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
                    console.log("Voice assistant activated via home button long press")

                    if (!voiceAssistant.isConnected) {
                        console.log("Voice assistant not connected, connecting first...")
                        voiceAssistant.connectToPhone("00:00:00:00:00:00")
                        connectionWaitTimer.start()
                    } else {
                        voiceControlLoader.active = true
                        voiceAssistant.activateAssistant()
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

            // Voice Control Overlay
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
        bottomMargin: 0   // ← Reset to 0
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
                    onLoaded: { item.theme = theme }
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
                    id: tidalLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "tidal"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Tidal.qml"
                    onLoaded: { item.theme = theme }
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
                    id: cameraLoader
                    anchors.fill: parent
                    visible: mainWindow.currentScreen === "camera"
                    active: false
                    asynchronous: true
                    source: "Ui/screens/Camera.qml"
                    onLoaded: { item.theme = theme }
                }
            }
        }
    }
}
