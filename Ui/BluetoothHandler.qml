import QtQuick 2.15
import HeadUnit

/**
 * BluetoothHandler - Encapsulates Bluetooth connection management.
 *
 * Handles auto-connect on startup, device event notifications,
 * service setup (media, notifications, voice), and call state
 * audio source management.
 */
Item {
    id: root
    anchors.fill: parent

    property var theme: null
    property var appSettings: null
    property string activeAudioSource: "none"

    // Emitted when a notification should be shown
    signal notificationRequested(var notification)

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
                root.setupDeviceServices(connectedAddress)
            } else if (appSettings && appSettings.autoConnectBluetooth) {
                var pairedAddress = bluetoothManager.getFirstPairedDeviceAddress()
                if (pairedAddress !== "") {
                    console.log("Auto-connect enabled: connecting to paired device:", pairedAddress)
                    bluetoothManager.connectToDevice(pairedAddress)
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
            root.showNotification("Connected to " + deviceName, "success")
            root.setupDeviceServices(address)
        }

        function onDeviceDisconnected(address) {
            var deviceName = bluetoothManager.getDeviceName(address)
            root.showNotification(deviceName + " disconnected", "info")
        }

        function onDevicePaired(address) {
            var deviceName = bluetoothManager.getDeviceName(address)
            root.showNotification(deviceName + " paired successfully", "success")
        }

        function onError(message) {
            root.showNotification(message, "error")
        }

        function onActiveCallChanged() {
            console.log("Call state changed - hasActiveCall:", bluetoothManager.hasActiveCall)

            if (bluetoothManager.hasActiveCall) {
                console.log("Call started - pausing music")
                mediaController.pause()
            } else {
                console.log("Call ended - resuming music")
                if (root.activeAudioSource === "music") {
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
        root.notificationRequested(notification)
    }

    // Timer to wait for voice connection
    Timer {
        id: connectionWaitTimer
        interval: 1500
        running: false
        repeat: false
        onTriggered: {
            if (voiceAssistant.isConnected) {
                // Voice pipeline handles this via its own mechanisms
                console.log("Voice assistant connected after wait")
            } else {
                console.log("Voice assistant connection failed")
            }
        }
    }
}
