import QtQuick 2.15

Item {
    id: root
    width: 400
    height: 40

    required property var theme

    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16

    // Real data from Bluetooth and sensors
    property int temperature: 22
    property int heading: 0

    // Use direct property bindings for reactive updates
    readonly property int phoneSignal: bluetoothManager?.cellularSignal ?? 0
    readonly property int phoneBattery: notificationManager?.phoneBatteryLevel ?? -1
    readonly property bool isCharging: bluetoothManager?.isConnectedDeviceCharging() ?? false
    readonly property string carrier: bluetoothManager?.carrierName ?? ""

    property string currentTime: "12:00"

    // Update time every minute
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            var now = new Date()
            var hours = now.getHours()
            var minutes = now.getMinutes()
            var ampm = hours >= 12 ? "PM" : "AM"
            hours = hours % 12
            hours = hours ? hours : 12 // 0 should be 12
            var minutesStr = minutes < 10 ? "0" + minutes : minutes
            currentTime = hours + ":" + minutesStr + " " + ampm
        }
    }

    // Update sensor data periodically (temperature and heading remain mock for now)
    Timer {
        interval: 5000
        running: true
        repeat: true
        onTriggered: {
            temperature = 18 + Math.floor(Math.random() * 10)
            heading = (heading + Math.floor(Math.random() * 30) - 15 + 360) % 360
            // Phone signal and battery now come from real Bluetooth data
        }
    }

    Component.onCompleted: {
        // Set initial time
        var now = new Date()
        var hours = now.getHours()
        var minutes = now.getMinutes()
        var ampm = hours >= 12 ? "PM" : "AM"
        hours = hours % 12
        hours = hours ? hours : 12
        var minutesStr = minutes < 10 ? "0" + minutes : minutes
        currentTime = hours + ":" + minutesStr + " " + ampm
    }

    Row {
        anchors.fill: parent
        spacing: 24

        // Time
        Text {
            text: currentTime
            color: textCol
            font.pixelSize: fontSize - 1
            font.family: fontFamily
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }

        Rectangle {
            width: 1
            height: 16
            color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
            anchors.verticalCenter: parent.verticalCenter
        }

        // Temperature
        Row {
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: temperature + "°C"
                color: textCol
                font.pixelSize: fontSize - 4
                font.family: fontFamily
                opacity: 0.8
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            width: 1
            height: 16
            color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
            anchors.verticalCenter: parent.verticalCenter
        }

        // Direction/Compass
        Row {
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: getCardinalDirection()
                color: textCol
                font.pixelSize: fontSize - 4
                font.family: fontFamily
                opacity: 0.8
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Rectangle {
            width: 1
            height: 16
            color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
            anchors.verticalCenter: parent.verticalCenter
        }

        // Phone Signal
        Row {
            spacing: 3
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: 4
                Rectangle {
                    width: 4
                    height: 5 + (index * 3)
                    color: index < phoneSignal ?
                        primaryCol : Qt.rgba(textCol.r, textCol.g, textCol.b, 0.2)
                    radius: 1
                    anchors.bottom: parent.bottom
                }
            }
        }

        Rectangle {
            width: 1
            height: 16
            color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
            anchors.verticalCenter: parent.verticalCenter
        }

        // Phone Battery
        Row {
            spacing: 6
            anchors.verticalCenter: parent.verticalCenter
            visible: phoneBattery >= 0  // Only show if battery info is available

            Rectangle {
                width: 24
                height: 12
                color: "transparent"
                border.color: getBatteryColor()
                border.width: 1.5
                radius: 2
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 2
                    width: Math.max(1, (parent.width - 4) * (Math.max(0, phoneBattery) / 100))
                    color: getBatteryColor()
                    radius: 1
                }

                Rectangle {
                    anchors.left: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: 6
                    color: getBatteryColor()
                    radius: 1
                }

                // Charging indicator (lightning bolt)
                Text {
                    visible: isCharging
                    anchors.centerIn: parent
                    text: "⚡"
                    color: "#ffff00"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }
            }

            Text {
                text: phoneBattery + "%"
                color: getBatteryColor()
                font.pixelSize: fontSize - 4
                font.family: fontFamily
                opacity: 0.8
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // Show message when battery not available
        Text {
            visible: phoneBattery < 0
            text: "No Device"
            color: textCol
            font.pixelSize: fontSize - 5
            font.family: fontFamily
            opacity: 0.5
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    function getCardinalDirection() {
        const directions = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
        const index = Math.round(heading / 45) % 8
        return directions[index]
    }

    function getBatteryColor() {
        if (phoneBattery < 0) return textCol  // No battery info
        if (phoneBattery <= 20) return "#ff0000"
        if (phoneBattery <= 50) return "#ffaa00"
        return primaryCol
    }
}
