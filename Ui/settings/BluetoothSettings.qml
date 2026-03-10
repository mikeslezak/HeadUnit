import QtQuick 2.15
import HeadUnit

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: col.height
    contentWidth: parent.width
    clip: true

    property var theme: null
    property var appSettings: null

    Column {
        id: col
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
                    if (bluetoothManager.isScanning) bluetoothManager.stopScan()
                    else bluetoothManager.startScan()
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
            isOn: appSettings?.autoConnectBluetooth ?? true
            onToggled: {
                if (appSettings) appSettings.autoConnectBluetooth = !appSettings.autoConnectBluetooth
            }
        }

        Rectangle {
            width: parent.width; height: 1
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        }

        // Status header
        Row {
            width: parent.width; spacing: 8
            Text {
                text: bluetoothManager.isScanning ? "🔍 Scanning..." : "Devices"
                color: ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
            }
            Text {
                text: "(" + bluetoothManager.deviceCount + ")"
                color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; opacity: 0.6
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Device list
        Column {
            width: parent.width
            spacing: 8

            Repeater {
                model: bluetoothManager.deviceModel

                BluetoothDeviceItem {
                    width: parent.width
                    deviceName: model.name || "Unknown Device"
                    deviceAddress: model.address || ""
                    isPaired: model.paired || false
                    isConnected: model.connected || false
                    signalStrength: model.rssi || -100
                }
            }
        }

        // Empty state
        Text {
            visible: bluetoothManager.deviceCount === 0
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: bluetoothManager.isScanning ? "Scanning..." : "No devices found\nTap 'Scan for Devices' to search"
            color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; opacity: 0.5
            wrapMode: Text.WordWrap
        }
    }

    // --- Inline Components ---

    component SettingButton: Rectangle {
        width: parent.width; height: 48
        color: destructive ? Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.2) : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        border.color: destructive ? ThemeValues.errorCol : ThemeValues.primaryCol; border.width: 2; radius: 8
        property string text: ""; property bool destructive: false
        signal clicked()
        Text { anchors.centerIn: parent; text: parent.text; color: destructive ? ThemeValues.errorCol : ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; font.weight: Font.Bold }
        MouseArea { anchors.fill: parent; onClicked: parent.clicked() }
    }

    component SettingToggle: Rectangle {
        width: parent.width; height: 60; color: "transparent"
        property string title: ""; property string description: ""; property bool isOn: false
        signal toggled()
        Column {
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; spacing: 4; width: parent.width - 80
            Text { text: title; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize + 1; font.family: ThemeValues.fontFamily; font.weight: Font.Bold }
            Text { text: description; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily; opacity: 0.6; wrapMode: Text.WordWrap; width: parent.width }
        }
        Rectangle {
            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            width: 60; height: 30; color: isOn ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
            radius: 15; border.color: isOn ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5); border.width: 2
            Rectangle { width: 22; height: 22; radius: 11; color: "white"; x: isOn ? parent.width - width - 4 : 4; anchors.verticalCenter: parent.verticalCenter; Behavior on x { NumberAnimation { duration: 150 } } }
            MouseArea { anchors.fill: parent; onClicked: parent.parent.toggled() }
        }
    }

    component BluetoothDeviceItem: Rectangle {
        height: 100
        color: isConnected ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15) : Qt.rgba(0, 0, 0, 0.3)
        border.color: isConnected ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
        border.width: isConnected ? 2 : 1; radius: 8
        property string deviceName: ""; property string deviceAddress: ""
        property bool isPaired: false; property bool isConnected: false; property int signalStrength: -100

        Row {
            anchors.fill: parent; anchors.margins: 16; spacing: 16

            Rectangle {
                width: 48; height: 48; radius: 24
                color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                border.color: ThemeValues.primaryCol; border.width: 2; anchors.verticalCenter: parent.verticalCenter
                Text { anchors.centerIn: parent; text: isPaired ? "📱" : "🔍"; font.pixelSize: 28 }
                Rectangle {
                    visible: isConnected; width: 16; height: 16; radius: 8; color: ThemeValues.successCol
                    border.color: "white"; border.width: 2; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: -4
                    SequentialAnimation on opacity {
                        running: isConnected; loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.3; duration: 1000 }
                        NumberAnimation { from: 0.3; to: 1.0; duration: 1000 }
                    }
                }
            }

            Column {
                width: parent.width - 270; anchors.verticalCenter: parent.verticalCenter; spacing: 6
                Text { text: deviceName; color: isConnected ? ThemeValues.primaryCol : ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize + 3; font.family: ThemeValues.fontFamily; font.weight: Font.Bold; elide: Text.ElideRight; width: parent.width }
                Text { visible: !isPaired; text: deviceAddress; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily; opacity: 0.5 }
                Row {
                    spacing: 10
                    Text { text: isConnected ? "● Connected" : (isPaired ? "● Paired" : "○ Not Paired"); color: isConnected ? ThemeValues.successCol : (isPaired ? ThemeValues.primaryCol : ThemeValues.textCol); font.pixelSize: ThemeValues.fontSize - 1; font.family: ThemeValues.fontFamily; opacity: 0.9 }
                    Text { visible: isPaired && signalStrength > -100; text: "📶"; font.pixelSize: ThemeValues.fontSize - 1; opacity: signalStrength > -70 ? 1.0 : signalStrength > -80 ? 0.7 : 0.4; anchors.verticalCenter: parent.verticalCenter }
                }
            }

            Rectangle {
                width: 180; height: 48; anchors.verticalCenter: parent.verticalCenter
                color: isConnected ? Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.2) : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                border.color: isConnected ? ThemeValues.errorCol : ThemeValues.primaryCol; border.width: 2; radius: 8
                property string buttonText: isConnected ? "Disconnect" : (isPaired ? "Connect" : "Pair")
                Text { anchors.centerIn: parent; text: parent.buttonText; color: isConnected ? ThemeValues.errorCol : ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize - 1; font.family: ThemeValues.fontFamily; font.weight: Font.Bold; horizontalAlignment: Text.AlignHCenter; width: parent.width - 20 }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (isConnected) { bluetoothManager.disconnectDevice(deviceAddress); if (mediaController.isConnected) mediaController.disconnect() }
                        else if (isPaired) { bluetoothManager.connectToDevice(deviceAddress); mediaController.connectToDevice(deviceAddress); if (appSettings) appSettings.lastBluetoothDevice = deviceName }
                        else { bluetoothManager.pairDevice(deviceAddress) }
                    }
                }
            }
        }
    }
}
