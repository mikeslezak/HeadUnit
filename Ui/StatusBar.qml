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

    // Mock data (would come from real sensors/GPS/Bluetooth in production)
    property int temperature: 22
    property int heading: 0
    property int phoneSignal: 4  // 0-5 bars
    property int phoneBattery: 85  // 0-100%
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

    // Update mock sensor data periodically
    Timer {
        interval: 5000
        running: true
        repeat: true
        onTriggered: {
            temperature = 18 + Math.floor(Math.random() * 10)
            heading = (heading + Math.floor(Math.random() * 30) - 15 + 360) % 360
            phoneSignal = Math.max(0, Math.min(5, phoneSignal + Math.floor(Math.random() * 3) - 1))
            if (Math.random() > 0.7) {
                phoneBattery = Math.max(10, phoneBattery - 1)
            }
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
        spacing: 20

        // Time
        Text {
            text: currentTime
            color: textCol
            font.pixelSize: fontSize - 2
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
            spacing: 2
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: 4
                Rectangle {
                    width: 3
                    height: 4 + (index * 2.5)
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
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                width: 20
                height: 10
                color: "transparent"
                border.color: getBatteryColor()
                border.width: 1
                radius: 1
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.margins: 1.5
                    width: Math.max(1, (parent.width - 3) * (phoneBattery / 100))
                    color: getBatteryColor()
                    radius: 0.5
                }

                Rectangle {
                    anchors.left: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: 5
                    color: getBatteryColor()
                    radius: 0.5
                }
            }

            Text {
                text: phoneBattery + "%"
                color: getBatteryColor()
                font.pixelSize: fontSize - 5
                font.family: fontFamily
                opacity: 0.8
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    function getCardinalDirection() {
        const directions = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
        const index = Math.round(heading / 45) % 8
        return directions[index]
    }

    function getBatteryColor() {
        if (phoneBattery <= 20) return "#ff0000"
        if (phoneBattery <= 50) return "#ffaa00"
        return primaryCol
    }
}
