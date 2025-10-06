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
            Column {
                visible: currentMenu === 1
                anchors.centerIn: parent
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

            // BLUETOOTH MENU
            Column {
                visible: currentMenu === 2
                anchors.centerIn: parent
                spacing: 20
                width: parent.width - 40

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

                Column {
                    width: parent.width
                    spacing: 12

                    Text {
                        text: "Paired Devices"
                        color: primaryCol
                        font.pixelSize: fontSize + 2
                        font.family: fontFamily
                        font.weight: Font.Bold
                    }

                    Text {
                        text: appSettings.lastBluetoothDevice || "No devices paired"
                        color: textCol
                        font.pixelSize: fontSize
                        font.family: fontFamily
                        opacity: 0.7
                    }

                    SettingButton {
                        text: "Scan for Devices"
                        onClicked: console.log("Scanning for Bluetooth devices...")
                    }

                    SettingButton {
                        text: "Forget All Devices"
                        destructive: true
                        onClicked: {
                            appSettings.lastBluetoothDevice = ""
                            console.log("All devices forgotten")
                        }
                    }
                }
            }

            // VOICE ASSISTANT MENU
            Column {
                visible: currentMenu === 3
                anchors.centerIn: parent
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

            // TIME & DATE MENU
            Column {
                visible: currentMenu === 4
                anchors.centerIn: parent
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

            // ABOUT MENU
            Column {
                visible: currentMenu === 5
                anchors.centerIn: parent
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

            // ADVANCED MENU
            Column {
                visible: currentMenu === 6
                anchors.centerIn: parent
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
}
