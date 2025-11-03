import QtQuick 2.15
import QtQuick.Effects
import QtCore

Item {
    id: root
    anchors.fill: parent
    property var theme: null

    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color accentCol: theme?.palette?.accent ?? "#ff006e"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16

    property int currentMenu: 0  // 0=Main, 1=Display, 2=Bluetooth, 3=Voice, 4=Time, 5=About

    Settings {
        id: appSettings
        property bool autoReadMessages: true
        property int voiceVolume: 80
        property bool use24HourFormat: false
        property bool autoConnectBluetooth: true
        property string lastBluetoothDevice: ""
    }

    Rectangle {
        anchors.fill: parent
        color: bgCol

        // Header with back button
        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 60
            color: Qt.rgba(0, 0, 0, 0.3)
            border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
            border.width: 1

            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // Back button
                Rectangle {
                    width: 50
                    height: 36
                    color: "transparent"
                    border.color: primaryCol
                    border.width: 2
                    radius: 6
                    visible: currentMenu !== 0
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "←"
                        color: primaryCol
                        font.pixelSize: 24
                        font.family: fontFamily
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: currentMenu = 0
                    }
                }

                Text {
                    text: getMenuTitle()
                    color: textCol
                    font.pixelSize: fontSize + 6
                    font.family: fontFamily
                    font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Content area
        Item {
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 12

            // MAIN MENU
            Grid {
                id: mainMenu
                visible: currentMenu === 0
                anchors.centerIn: parent
                columns: 3
                rows: 2
                columnSpacing: 16
                rowSpacing: 16

                SettingCard {
                    title: "Display"
                    icon: "🎨"
                    description: "Theme & Appearance"
                    onClicked: currentMenu = 1
                }

                SettingCard {
                    title: "Bluetooth"
                    icon: "📡"
                    description: "Devices & Pairing"
                    onClicked: currentMenu = 2
                }

                SettingCard {
                    title: "Voice Assistant"
                    icon: "🎤"
                    description: "Voice Controls"
                    onClicked: currentMenu = 3
                }

                SettingCard {
                    title: "Time & Date"
                    icon: "🕐"
                    description: "Clock Settings"
                    onClicked: currentMenu = 4
                }

                SettingCard {
                    title: "About"
                    icon: "ℹ"
                    description: "System Info"
                    onClicked: currentMenu = 5
                }

                SettingCard {
                    title: "Advanced"
                    icon: "⚙"
                    description: "Developer Options"
                    onClicked: currentMenu = 6
                }
            }

            // DISPLAY MENU
            Flickable {
                visible: currentMenu === 1
                anchors.fill: parent
                contentHeight: displayColumn.height
                clip: true

                Column {
                    id: displayColumn
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    spacing: 16
                    width: parent.width - 40

                    Text {
                        text: "Select Theme"
                        color: primaryCol
                        font.pixelSize: fontSize + 2
                        font.family: fontFamily
                        font.weight: Font.Bold
                    }

                    Row {
                        spacing: 16
                        anchors.horizontalCenter: parent.horizontalCenter

                        ThemeOption {
                            themeName: "Cyberpunk"
                            description: "Neon Future"
                            isActive: theme?.name === "Cyberpunk"
                            onClicked: {
                                if (theme && theme.load) {
                                    theme.load("Cyberpunk")
                                }
                            }
                        }

                        ThemeOption {
                            themeName: "RetroVFD"
                            description: "Classic Display"
                            isActive: theme?.name === "RetroVFD"
                            onClicked: {
                                if (theme && theme.load) {
                                    theme.load("RetroVFD")
                                }
                            }
                        }
                    }
                }
            }

            // BLUETOOTH MENU
            Flickable {
                visible: currentMenu === 2
                anchors.fill: parent
                contentHeight: bluetoothColumn.height
                contentWidth: parent.width
                clip: true

                Column {
                    id: bluetoothColumn
                    width: parent.width
                    spacing: 16

                    // Header controls
                    Row {
                        width: parent.width
                        spacing: 12

                        SettingButton {
                            width: (parent.width - 12) / 2
                            text: bluetoothManager.isScanning ? "Stop Scan" : "Scan for Devices"
                            onClicked: {
                                if (bluetoothManager.isScanning) {
                                    bluetoothManager.stopScan()
                                } else {
                                    bluetoothManager.startScan()
                                }
                            }
                        }

                        SettingButton {
                            width: (parent.width - 12) / 2
                            text: "Refresh List"
                            onClicked: bluetoothManager.refreshDeviceList()
                        }
                    }

                    SettingToggle {
                        title: "Auto-Connect"
                        description: "Automatically connect to last device"
                        isOn: appSettings.autoConnectBluetooth
                        onToggled: appSettings.autoConnectBluetooth = !appSettings.autoConnectBluetooth
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                    }

                    // Status header
                    Row {
                        width: parent.width
                        spacing: 8

                        Text {
                            text: bluetoothManager.isScanning ? "🔍 Scanning..." : "📡 Devices"
                            color: primaryCol
                            font.pixelSize: fontSize + 2
                            font.family: fontFamily
                            font.weight: Font.Bold
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "(" + bluetoothManager.deviceCount + ")"
                            color: textCol
                            font.pixelSize: fontSize
                            font.family: fontFamily
                            opacity: 0.6
                            anchors.verticalCenter: parent.verticalCenter

                            Component.onCompleted: {
                                console.log("BT Settings: Device count =", bluetoothManager.deviceCount)
                                console.log("BT Settings: Model valid =", bluetoothManager.deviceModel !== null)
                            }
                        }

                        // Scanning animation
                        Rectangle {
                            visible: bluetoothManager.isScanning
                            width: 12
                            height: 12
                            radius: 6
                            color: primaryCol
                            anchors.verticalCenter: parent.verticalCenter

                            SequentialAnimation on opacity {
                                running: bluetoothManager.isScanning
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.2; duration: 800 }
                                NumberAnimation { from: 0.2; to: 1.0; duration: 800 }
                            }
                        }
                    }

                    // Device List
                    Rectangle {
                        width: parent.width
                        height: deviceListColumn.height + 16
                        color: Qt.rgba(0, 0, 0, 0.2)
                        border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                        border.width: 1
                        radius: 8

                        Column {
                            id: deviceListColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 8
                            spacing: 8

                            Repeater {
                                model: bluetoothManager.deviceModel

                                BluetoothDeviceItem {
                                    width: deviceListColumn.width
                                    deviceName: model.deviceName
                                    deviceAddress: model.deviceAddress
                                    isPaired: model.isPaired
                                    isConnected: model.isConnected
                                    signalStrength: model.signalStrength
                                }
                            }

                            Text {
                                visible: bluetoothManager.deviceCount === 0
                                width: parent.width
                                height: 100
                                text: bluetoothManager.isScanning ? "Scanning..." : "No devices found\nTap 'Scan for Devices' to search"
                                color: textCol
                                font.pixelSize: fontSize
                                font.family: fontFamily
                                opacity: 0.5
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    // Status message
                    Text {
                        width: parent.width
                        text: bluetoothManager.statusMessage
                        color: textCol
                        font.pixelSize: fontSize - 2
                        font.family: fontFamily
                        opacity: 0.6
                        elide: Text.ElideRight
                    }
                }
            }

            // VOICE ASSISTANT MENU
            Flickable {
                visible: currentMenu === 3
                anchors.fill: parent
                contentHeight: voiceColumn.height
                clip: true

                Column {
                    id: voiceColumn
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    spacing: 20
                    width: parent.width - 40

                SettingToggle {
                    title: "Auto-Read Messages"
                    description: "Read incoming messages aloud while driving"
                    isOn: appSettings.autoReadMessages
                    onToggled: {
                        appSettings.autoReadMessages = !appSettings.autoReadMessages
                        voiceAssistant.setAutoReadMessages(appSettings.autoReadMessages)
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                }

                Column {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "Voice Volume"
                        color: primaryCol
                        font.pixelSize: fontSize + 2
                        font.family: fontFamily
                        font.weight: Font.Bold
                    }

                    Row {
                        spacing: 12
                        width: parent.width

                        Text {
                            text: "🔉"
                            font.pixelSize: 20
                            color: primaryCol
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Rectangle {
                            width: parent.width - 100
                            height: 8
                            color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                            radius: 4
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                width: parent.width * (appSettings.voiceVolume / 100)
                                height: parent.height
                                color: primaryCol
                                radius: 4
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    appSettings.voiceVolume = Math.floor((mouse.x / width) * 100)
                                    voiceAssistant.setVoiceVolume(appSettings.voiceVolume)
                                }
                            }
                        }

                        Text {
                            text: appSettings.voiceVolume + "%"
                            color: textCol
                            font.pixelSize: fontSize
                            font.family: fontFamily
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                }

                Text {
                    text: "Quick Replies"
                    color: primaryCol
                    font.pixelSize: fontSize + 2
                    font.family: fontFamily
                    font.weight: Font.Bold
                }

                Column {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: voiceAssistant.quickReplies.slice(0, 3)

                        Rectangle {
                            width: parent.width
                            height: 40
                            color: Qt.rgba(0, 0, 0, 0.3)
                            border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.5)
                            border.width: 1
                            radius: 6

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                color: textCol
                                font.pixelSize: fontSize - 2
                                font.family: fontFamily
                            }
                        }
                    }
                }
                }
            }

            // TIME & DATE MENU
            Flickable {
                visible: currentMenu === 4
                anchors.fill: parent
                contentHeight: timeColumn.height
                clip: true

                Column {
                    id: timeColumn
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    spacing: 20
                    width: parent.width - 40

                SettingToggle {
                    title: "24-Hour Format"
                    description: "Use 24-hour time format"
                    isOn: appSettings.use24HourFormat
                    onToggled: appSettings.use24HourFormat = !appSettings.use24HourFormat
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                }

                Row {
                    width: parent.width
                    spacing: 12

                    Text {
                        text: "Current Time:"
                        color: primaryCol
                        font.pixelSize: fontSize + 2
                        font.family: fontFamily
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: Qt.formatDateTime(new Date(), appSettings.use24HourFormat ? "HH:mm:ss" : "h:mm:ss AP")
                        color: textCol
                        font.pixelSize: fontSize + 4
                        font.family: fontFamily
                        font.weight: Font.Bold
                        anchors.verticalCenter: parent.verticalCenter

                        Timer {
                            interval: 1000
                            running: currentMenu === 4
                            repeat: true
                            onTriggered: parent.text = Qt.formatDateTime(new Date(), appSettings.use24HourFormat ? "HH:mm:ss" : "h:mm:ss AP")
                        }
                    }
                }
                }
            }

            // ABOUT MENU
            Flickable {
                visible: currentMenu === 5
                anchors.fill: parent
                contentHeight: aboutColumn.height
                clip: true

                Column {
                    id: aboutColumn
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    spacing: 20
                    width: parent.width - 40

                Rectangle {
                    width: 120
                    height: 120
                    color: "transparent"
                    border.color: primaryCol
                    border.width: 2
                    radius: 60
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "🚗"
                        font.pixelSize: 60
                    }
                }

                Text {
                    text: "HeadUnit"
                    color: textCol
                    font.pixelSize: fontSize + 8
                    font.family: fontFamily
                    font.weight: Font.Bold
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Column {
                    width: parent.width
                    spacing: 12
                    anchors.horizontalCenter: parent.horizontalCenter

                    InfoRow { label: "Version"; value: "1.0.0" }
                    InfoRow { label: "Build"; value: "2025.10.05" }
                    InfoRow { label: "Qt Version"; value: "6.9" }
                    InfoRow { label: "Platform"; value: Qt.platform.os }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                }

                Text {
                    text: "© 2025 Custom Head Unit Project"
                    color: textCol
                    font.pixelSize: fontSize - 2
                    font.family: fontFamily
                    opacity: 0.5
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                }
            }

            // ADVANCED MENU
            Flickable {
                visible: currentMenu === 6
                anchors.fill: parent
                contentHeight: advancedColumn.height
                clip: true

                Column {
                    id: advancedColumn
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    spacing: 20
                    width: parent.width - 40

                Text {
                    text: "⚠️ Advanced Settings"
                    color: accentCol
                    font.pixelSize: fontSize + 2
                    font.family: fontFamily
                    font.weight: Font.Bold
                }

                SettingButton {
                    text: "Factory Reset"
                    destructive: true
                    onClicked: console.log("Factory reset requested")
                }

                SettingButton {
                    text: "Clear Cache"
                    onClicked: console.log("Cache cleared")
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                }

                Column {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: "Developer Info"
                        color: primaryCol
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        font.weight: Font.Bold
                    }

                    InfoRow { label: "Theme"; value: theme?.name ?? "Unknown" }
                    InfoRow { label: "Text-to-Speech"; value: voiceAssistant.hasTextToSpeech ? "Available" : "Not Available" }
                    InfoRow { label: "Media Connected"; value: mediaController.isConnected ? "Yes" : "No" }
                }
                }
            }
        }
    }

    function getMenuTitle() {
        switch(currentMenu) {
            case 0: return "Settings"
            case 1: return "Display"
            case 2: return "Bluetooth"
            case 3: return "Voice Assistant"
            case 4: return "Time & Date"
            case 5: return "About"
            case 6: return "Advanced"
            default: return "Settings"
        }
    }

    // Setting Card Component
    component SettingCard: Rectangle {
        width: 260
        height: 110
        color: Qt.rgba(0, 0, 0, 0.3)
        border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.5)
        border.width: 2
        radius: 8

        property string title: ""
        property string icon: ""
        property string description: ""

        signal clicked()

        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: icon
                font.pixelSize: 36
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: title
                color: textCol
                font.pixelSize: fontSize + 2
                font.family: fontFamily
                font.weight: Font.Bold
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: description
                color: textCol
                font.pixelSize: fontSize - 3
                font.family: fontFamily
                opacity: 0.6
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: parent.clicked()
        }
    }

    // Theme Option Component
    component ThemeOption: Rectangle {
        width: 200
        height: 80
        color: isActive ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2) : Qt.rgba(0, 0, 0, 0.3)
        border.color: isActive ? primaryCol : Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.5)
        border.width: 2
        radius: 8

        property string themeName: ""
        property string description: ""
        property bool isActive: false

        signal clicked()

        Column {
            anchors.centerIn: parent
            spacing: 4

            Text {
                text: themeName
                color: isActive ? primaryCol : textCol
                font.pixelSize: fontSize + 2
                font.family: fontFamily
                font.weight: Font.Bold
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: description
                color: textCol
                font.pixelSize: fontSize - 2
                font.family: fontFamily
                opacity: 0.6
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: parent.clicked()
        }
    }

    // Setting Toggle Component
    component SettingToggle: Rectangle {
        width: parent.width
        height: 60
        color: "transparent"

        property string title: ""
        property string description: ""
        property bool isOn: false

        signal toggled()

        Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            width: parent.width - 80

            Text {
                text: title
                color: textCol
                font.pixelSize: fontSize + 1
                font.family: fontFamily
                font.weight: Font.Bold
            }

            Text {
                text: description
                color: textCol
                font.pixelSize: fontSize - 2
                font.family: fontFamily
                opacity: 0.6
                wrapMode: Text.WordWrap
                width: parent.width
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 60
            height: 30
            color: isOn ? primaryCol : Qt.rgba(textCol.r, textCol.g, textCol.b, 0.3)
            radius: 15
            border.color: isOn ? primaryCol : Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
            border.width: 2

            Rectangle {
                width: 22
                height: 22
                radius: 11
                color: "white"
                x: isOn ? parent.width - width - 4 : 4
                anchors.verticalCenter: parent.verticalCenter

                Behavior on x { NumberAnimation { duration: 150 } }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: parent.parent.toggled()
            }
        }
    }

    // Setting Button Component
    component SettingButton: Rectangle {
        width: parent.width
        height: 48
        color: destructive ? Qt.rgba(1, 0, 0, 0.2) : Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
        border.color: destructive ? "#ff0000" : primaryCol
        border.width: 2
        radius: 8

        property string text: ""
        property bool destructive: false

        signal clicked()

        Text {
            anchors.centerIn: parent
            text: parent.text
            color: destructive ? "#ff0000" : primaryCol
            font.pixelSize: fontSize
            font.family: fontFamily
            font.weight: Font.Bold
        }

        MouseArea {
            anchors.fill: parent
            onClicked: parent.clicked()
        }
    }

    // Info Row Component
    component InfoRow: Row {
        width: parent.width
        spacing: 12

        property string label: ""
        property string value: ""

        Text {
            text: label + ":"
            color: primaryCol
            font.pixelSize: fontSize
            font.family: fontFamily
            width: 120
        }

        Text {
            text: value
            color: textCol
            font.pixelSize: fontSize
            font.family: fontFamily
        }
    }

    // Bluetooth Device Item Component
    component BluetoothDeviceItem: Rectangle {
        height: 80
        color: isConnected ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.15) : Qt.rgba(0, 0, 0, 0.3)
        border.color: isConnected ? primaryCol : Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.4)
        border.width: isConnected ? 2 : 1
        radius: 8

        property string deviceName: ""
        property string deviceAddress: ""
        property bool isPaired: false
        property bool isConnected: false
        property int signalStrength: -100

        Row {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // Device icon
            Rectangle {
                width: 56
                height: 56
                radius: 28
                color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                border.color: primaryCol
                border.width: 2
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: isPaired ? "📱" : "🔍"
                    font.pixelSize: 28
                }

                // Connection indicator
                Rectangle {
                    visible: isConnected
                    width: 16
                    height: 16
                    radius: 8
                    color: "#00ff00"
                    border.color: "white"
                    border.width: 2
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: -4

                    SequentialAnimation on opacity {
                        running: isConnected
                        loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.3; duration: 1000 }
                        NumberAnimation { from: 0.3; to: 1.0; duration: 1000 }
                    }
                }
            }

            // Device info
            Column {
                width: parent.width - 200
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    text: deviceName
                    color: isConnected ? primaryCol : textCol
                    font.pixelSize: fontSize + 2
                    font.family: fontFamily
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    width: parent.width
                }

                Text {
                    text: deviceAddress
                    color: textCol
                    font.pixelSize: fontSize - 3
                    font.family: fontFamily
                    opacity: 0.6
                }

                Row {
                    spacing: 8

                    Text {
                        text: isConnected ? "● Connected" : (isPaired ? "● Paired" : "○ Not Paired")
                        color: isConnected ? "#00ff00" : (isPaired ? primaryCol : textCol)
                        font.pixelSize: fontSize - 2
                        font.family: fontFamily
                        opacity: 0.8
                    }

                    // Signal strength indicator
                    Row {
                        spacing: 2
                        visible: signalStrength > -100
                        anchors.verticalCenter: parent.verticalCenter

                        Repeater {
                            model: 4
                            Rectangle {
                                width: 3
                                height: 4 + (index * 3)
                                color: signalStrength > (-90 + index * 10) ? primaryCol : Qt.rgba(textCol.r, textCol.g, textCol.b, 0.3)
                                radius: 1
                                anchors.bottom: parent.bottom
                            }
                        }
                    }
                }
            }

            // Action buttons
            Column {
                width: 100
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Rectangle {
                    width: parent.width
                    height: 28
                    color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                    border.color: primaryCol
                    border.width: 1
                    radius: 6
                    visible: isPaired

                    Text {
                        anchors.centerIn: parent
                        text: isConnected ? "Disconnect" : "Connect"
                        color: primaryCol
                        font.pixelSize: fontSize - 3
                        font.family: fontFamily
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (isConnected) {
                                bluetoothManager.disconnectDevice(deviceAddress)
                                // Also disconnect from MediaController if this was the media device
                                if (mediaController.isConnected) {
                                    mediaController.disconnect()
                                }
                            } else {
                                bluetoothManager.connectToDevice(deviceAddress)
                                // Also connect MediaController for music control
                                mediaController.connectToDevice(deviceAddress)
                                // Save as last device
                                appSettings.lastBluetoothDevice = deviceName
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 28
                    color: isPaired ? Qt.rgba(1, 0, 0, 0.2) : Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                    border.color: isPaired ? "#ff0000" : primaryCol
                    border.width: 1
                    radius: 6

                    Text {
                        anchors.centerIn: parent
                        text: isPaired ? "Unpair" : "Pair"
                        color: isPaired ? "#ff0000" : primaryCol
                        font.pixelSize: fontSize - 3
                        font.family: fontFamily
                        font.weight: Font.Bold
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (isPaired) {
                                bluetoothManager.unpairDevice(deviceAddress)
                            } else {
                                bluetoothManager.pairDevice(deviceAddress)
                            }
                        }
                    }
                }
            }
        }
    }
}
