import QtQuick 2.15
import QtQuick.Effects
import QtCore
import HeadUnit

Item {
    id: root
    anchors.fill: parent
    property var theme: null
    property var appSettings: null


    property int currentMenu: 0  // 0=Main, 1=Display, 2=Bluetooth, 3=Voice, 4=Time, 5=About, 6=Advanced

    // Auto-refresh device list when connection state changes
    Connections {
        target: bluetoothManager

        function onDeviceConnected(address) {
            console.log("Settings: Device connected, refreshing list:", address)
            bluetoothManager.refreshDeviceList()
        }

        function onDeviceDisconnected(address) {
            console.log("Settings: Device disconnected, refreshing list:", address)
            bluetoothManager.refreshDeviceList()
        }

        function onDevicePaired(address) {
            console.log("Settings: Device paired, refreshing list:", address)
            bluetoothManager.refreshDeviceList()
        }

        function onDeviceUnpaired(address) {
            console.log("Settings: Device unpaired, refreshing list:", address)
            bluetoothManager.refreshDeviceList()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        // Header with back button
        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 60
            color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.3)
            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
            border.width: 1

            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                Rectangle {
                    width: 50; height: 36
                    color: "transparent"
                    border.color: ThemeValues.primaryCol; border.width: 2; radius: 6
                    visible: currentMenu !== 0
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "←"; color: ThemeValues.primaryCol; font.pixelSize: 24; font.family: ThemeValues.fontFamily
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: currentMenu = 0
                    }
                }

                Text {
                    text: getMenuTitle()
                    color: ThemeValues.textCol
                    font.pixelSize: ThemeValues.fontSize + 6; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
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
                columns: 3; rows: 2
                columnSpacing: 24; rowSpacing: 24

                SettingCard { title: "Display"; icon: "display"; description: "Theme & Appearance"; onClicked: currentMenu = 1 }
                SettingCard { title: "Bluetooth"; icon: "bluetooth"; description: "Devices & Pairing"; onClicked: currentMenu = 2 }
                SettingCard { title: "Voice Assistant"; icon: "voice"; description: "Voice Controls"; onClicked: currentMenu = 3 }
                SettingCard { title: "Time & Date"; icon: "time"; description: "Clock Settings"; onClicked: currentMenu = 4 }
                SettingCard { title: "About"; icon: "about"; description: "System Info"; onClicked: currentMenu = 5 }
                SettingCard { title: "Advanced"; icon: "advanced"; description: "Developer Options"; onClicked: currentMenu = 6 }
            }

            // Sub-page loaders
            Loader {
                anchors.fill: parent
                active: currentMenu === 1
                source: "../settings/DisplaySettings.qml"
                onLoaded: { item.theme = Qt.binding(function() { return root.theme }); item.appSettings = Qt.binding(function() { return root.appSettings }) }
            }

            Loader {
                anchors.fill: parent
                active: currentMenu === 2
                source: "../settings/BluetoothSettings.qml"
                onLoaded: { item.theme = Qt.binding(function() { return root.theme }); item.appSettings = Qt.binding(function() { return root.appSettings }) }
            }

            Loader {
                anchors.fill: parent
                active: currentMenu === 3
                source: "../settings/VoiceSettings.qml"
                onLoaded: { item.theme = Qt.binding(function() { return root.theme }); item.appSettings = Qt.binding(function() { return root.appSettings }) }
            }

            Loader {
                anchors.fill: parent
                active: currentMenu === 4
                source: "../settings/TimeSettings.qml"
                onLoaded: { item.theme = Qt.binding(function() { return root.theme }); item.appSettings = Qt.binding(function() { return root.appSettings }) }
            }

            Loader {
                anchors.fill: parent
                active: currentMenu === 5
                source: "../settings/AboutSettings.qml"
                onLoaded: { item.theme = Qt.binding(function() { return root.theme }); item.appSettings = Qt.binding(function() { return root.appSettings }) }
            }

            Loader {
                anchors.fill: parent
                active: currentMenu === 6
                source: "../settings/AdvancedSettings.qml"
                onLoaded: { item.theme = Qt.binding(function() { return root.theme }); item.appSettings = Qt.binding(function() { return root.appSettings }) }
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

    // Setting Card Component (only used in main menu)
    component SettingCard: Rectangle {
        width: 260; height: 110
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.3)
        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.5)
        border.width: 2; radius: 8
        property string title: ""; property string icon: ""; property string description: ""
        signal clicked()

        Column {
            anchors.centerIn: parent; spacing: 8
            Canvas {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 36; height: 36
                property string iconType: icon
                property color iconColor: ThemeValues.primaryCol
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = iconColor.toString()
                    ctx.fillStyle = iconColor.toString()
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    switch(iconType) {
                    case "display":
                        // Monitor/palette icon
                        ctx.beginPath(); ctx.roundedRect(4, 6, 28, 20, 3, 3); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(14, 26); ctx.lineTo(14, 31); ctx.moveTo(22, 26); ctx.lineTo(22, 31); ctx.moveTo(11, 31); ctx.lineTo(25, 31); ctx.stroke()
                        ctx.beginPath(); ctx.arc(12, 16, 3, 0, Math.PI * 2); ctx.fill()
                        ctx.beginPath(); ctx.arc(20, 14, 2, 0, Math.PI * 2); ctx.fill()
                        ctx.beginPath(); ctx.arc(26, 18, 1.5, 0, Math.PI * 2); ctx.fill()
                        break
                    case "bluetooth":
                        // Bluetooth rune
                        ctx.beginPath(); ctx.moveTo(14, 8); ctx.lineTo(22, 16); ctx.lineTo(14, 24); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(14, 24); ctx.lineTo(22, 16); ctx.lineTo(18, 12); ctx.lineTo(18, 28); ctx.lineTo(22, 24); ctx.lineTo(14, 16); ctx.stroke()
                        break
                    case "voice":
                        // Microphone
                        ctx.beginPath(); ctx.roundedRect(14, 6, 8, 14, 4, 4); ctx.stroke()
                        ctx.beginPath(); ctx.arc(18, 22, 8, 0, Math.PI); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(18, 30); ctx.lineTo(18, 34); ctx.moveTo(13, 34); ctx.lineTo(23, 34); ctx.stroke()
                        break
                    case "time":
                        // Clock
                        ctx.beginPath(); ctx.arc(18, 18, 12, 0, Math.PI * 2); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(18, 18); ctx.lineTo(18, 11); ctx.moveTo(18, 18); ctx.lineTo(24, 18); ctx.stroke()
                        break
                    case "about":
                        // Info circle
                        ctx.beginPath(); ctx.arc(18, 18, 12, 0, Math.PI * 2); ctx.stroke()
                        ctx.beginPath(); ctx.moveTo(18, 14); ctx.lineTo(18, 15); ctx.stroke(); ctx.lineWidth = 2
                        ctx.beginPath(); ctx.moveTo(18, 19); ctx.lineTo(18, 26); ctx.stroke()
                        break
                    case "advanced":
                        // Gear
                        ctx.beginPath(); ctx.arc(18, 18, 6, 0, Math.PI * 2); ctx.stroke()
                        for (var i = 0; i < 8; i++) {
                            var angle = (i * Math.PI * 2 / 8)
                            ctx.beginPath()
                            ctx.moveTo(18 + Math.cos(angle) * 8, 18 + Math.sin(angle) * 8)
                            ctx.lineTo(18 + Math.cos(angle) * 12, 18 + Math.sin(angle) * 12)
                            ctx.stroke()
                        }
                        break
                    }
                }
                onIconColorChanged: requestPaint()
            }
            Text { text: title; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold; anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: description; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 3; font.family: ThemeValues.fontFamily; opacity: 0.6; anchors.horizontalCenter: parent.horizontalCenter }
        }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
    }
}
