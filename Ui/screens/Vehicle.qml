import QtQuick 2.15
import HeadUnit

Item {
    id: root
    property var theme: null


    // Convenience: is ECM sending data?
    readonly property bool ecmLive: vehicleBusManager.ecmOnline

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        // ---- Not Connected State ----
        Column {
            anchors.centerIn: parent
            spacing: 16
            visible: !vehicleBusManager.connected

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Vehicle Bus Offline"
                color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize + 6; font.family: ThemeValues.fontFamily
                opacity: 0.6
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Connect CAN adapter to view engine data"
                color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily
                opacity: 0.3
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 140; height: 40; radius: 8
                color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                border.color: ThemeValues.primaryCol; border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Connect"
                    color: ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: vehicleBusManager.connectBus("can0")
                }
            }
        }

        // ---- Live Dashboard ----
        Flickable {
            anchors.fill: parent
            anchors.margins: 16
            contentHeight: dashCol.height
            clip: true
            visible: vehicleBusManager.connected

            Column {
                id: dashCol
                width: parent.width
                spacing: 16

                // Header
                Item {
                    width: parent.width
                    height: 36

                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12

                        Text {
                            text: "Engine"
                            color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize + 8; font.family: ThemeValues.fontFamily; font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // ECM status indicator
                        Rectangle {
                            width: 10; height: 10; radius: 5
                            color: ecmLive ? ThemeValues.successCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                            anchors.verticalCenter: parent.verticalCenter

                            SequentialAnimation on opacity {
                                running: ecmLive
                                loops: Animation.Infinite
                                NumberAnimation { from: 1.0; to: 0.4; duration: 800 }
                                NumberAnimation { from: 0.4; to: 1.0; duration: 800 }
                            }
                        }

                        Text {
                            text: ecmLive ? "ECM Online" : "ECM Offline"
                            color: ecmLive ? ThemeValues.successCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                            font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: 0.8
                        }
                    }

                    // Drive mode badge (right-aligned)
                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: modeText.width + 20; height: 28; radius: 6
                        color: {
                            switch(vehicleBusManager.driveMode) {
                                case 0: return Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.2)
                                case 2: return Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.2)
                                case 3: return Qt.rgba(ThemeValues.warningCol.r, ThemeValues.warningCol.g, ThemeValues.warningCol.b, 0.2)
                                case 4: return Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.15)
                                default: return Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                            }
                        }
                        border.color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.15); border.width: 1

                        Text {
                            id: modeText
                            anchors.centerIn: parent
                            text: ["ECO", "NORMAL", "SPORT", "TOW", "VALET"][vehicleBusManager.driveMode] || "NORMAL"
                            color: ThemeValues.textCol; font.pixelSize: 11; font.family: ThemeValues.fontFamily; font.bold: true
                        }
                    }
                }

                // ---- Primary Gauges Row ----
                Row {
                    width: parent.width
                    spacing: 8

                    GaugeCard {
                        width: (parent.width - 16) / 3; height: 120
                        label: "RPM"
                        value: vehicleBusManager.rpm.toLocaleString()
                        unit: ""
                        highlight: vehicleBusManager.rpm > 5500
                    }
                    GaugeCard {
                        width: (parent.width - 16) / 3; height: 120
                        label: "COOLANT"
                        value: vehicleBusManager.coolantTemp.toFixed(1)
                        unit: "°C"
                        highlight: vehicleBusManager.coolantTemp > 105
                    }
                    GaugeCard {
                        width: (parent.width - 16) / 3; height: 120
                        label: "OIL"
                        value: (vehicleBusManager.oilPressureKpa * 0.145038).toFixed(0)
                        unit: "PSI"
                        highlight: vehicleBusManager.oilPressureKpa < 140 && vehicleBusManager.rpm > 1000
                    }
                }

                // ---- Secondary Row ----
                Row {
                    width: parent.width
                    spacing: 8

                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "TPS"
                        value: vehicleBusManager.tps.toFixed(1)
                        unit: "%"
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "MAP"
                        value: vehicleBusManager.mapKpa.toString()
                        unit: "kPa"
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "TIMING"
                        value: vehicleBusManager.timing.toString()
                        unit: "°BTDC"
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "BATTERY"
                        value: vehicleBusManager.batteryVoltage.toFixed(1)
                        unit: "V"
                        highlight: vehicleBusManager.batteryVoltage < 12.0 || vehicleBusManager.batteryVoltage > 15.0
                    }
                }

                // ---- Fueling Row ----
                Row {
                    width: parent.width
                    spacing: 8

                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "AFR B1"
                        value: (vehicleBusManager.lambdaB1 * 14.7).toFixed(1)
                        unit: ""
                        highlight: vehicleBusManager.lambdaB1 < 0.75 || vehicleBusManager.lambdaB1 > 1.1
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "AFR B2"
                        value: (vehicleBusManager.lambdaB2 * 14.7).toFixed(1)
                        unit: ""
                        highlight: vehicleBusManager.lambdaB2 < 0.75 || vehicleBusManager.lambdaB2 > 1.1
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "VE"
                        value: vehicleBusManager.ve.toFixed(1)
                        unit: "%"
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "INJ DUTY"
                        value: vehicleBusManager.injectorDuty.toFixed(1)
                        unit: "%"
                        highlight: vehicleBusManager.injectorDuty > 85
                    }
                }

                // ---- Fuel Trims + Knock Row ----
                Row {
                    width: parent.width
                    spacing: 8

                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "STFT"
                        value: (vehicleBusManager.stft > 0 ? "+" : "") + vehicleBusManager.stft.toString()
                        unit: "%"
                        highlight: Math.abs(vehicleBusManager.stft) > 15
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "LTFT"
                        value: (vehicleBusManager.ltft > 0 ? "+" : "") + vehicleBusManager.ltft.toString()
                        unit: "%"
                        highlight: Math.abs(vehicleBusManager.ltft) > 10
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "KNOCK B1"
                        value: vehicleBusManager.knockB1.toString()
                        unit: ""
                        highlight: vehicleBusManager.knockB1 > 20
                    }
                    GaugeCard {
                        width: (parent.width - 24) / 4; height: 100
                        label: "KNOCK B2"
                        value: vehicleBusManager.knockB2.toString()
                        unit: ""
                        highlight: vehicleBusManager.knockB2 > 20
                    }
                }

                // ---- Pressures + IAT Row ----
                Row {
                    width: parent.width
                    spacing: 8

                    GaugeCard {
                        width: (parent.width - 16) / 3; height: 100
                        label: "FUEL PRESS"
                        value: (vehicleBusManager.fuelPressureKpa * 0.145038).toFixed(1)
                        unit: "PSI"
                        highlight: vehicleBusManager.fuelPressureKpa < 370 || vehicleBusManager.fuelPressureKpa > 435
                    }
                    GaugeCard {
                        width: (parent.width - 16) / 3; height: 100
                        label: "IAT"
                        value: vehicleBusManager.iatTemp.toFixed(1)
                        unit: "°C"
                        highlight: vehicleBusManager.iatTemp > 50
                    }
                    GaugeCard {
                        width: (parent.width - 16) / 3; height: 100
                        label: "FAULTS"
                        value: vehicleBusManager.faultCount.toString()
                        unit: ""
                        highlight: vehicleBusManager.faultCount > 0
                    }
                }

                // Module status footer
                Row {
                    width: parent.width
                    spacing: 16

                    Repeater {
                        model: [
                            { name: "ECM", online: vehicleBusManager.ecmOnline },
                            { name: "TCM", online: vehicleBusManager.tcmOnline },
                            { name: "PDCM", online: vehicleBusManager.pdcmOnline },
                            { name: "GCM", online: vehicleBusManager.gcmOnline }
                        ]

                        Row {
                            spacing: 6
                            Rectangle {
                                width: 8; height: 8; radius: 4
                                color: modelData.online ? ThemeValues.successCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.name
                                color: modelData.online ? ThemeValues.textCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                                font.pixelSize: 11; font.family: ThemeValues.fontFamily
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- Inline GaugeCard component ----
    component GaugeCard: Rectangle {
        property string label: ""
        property string value: "0"
        property string unit: ""
        property bool highlight: false

        color: highlight
            ? Qt.rgba(ThemeValues.accentCol.r, ThemeValues.accentCol.g, ThemeValues.accentCol.b, 0.12)
            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.06)
        radius: 10
        border.color: highlight
            ? Qt.rgba(ThemeValues.accentCol.r, ThemeValues.accentCol.g, ThemeValues.accentCol.b, 0.4)
            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 4

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: label
                color: highlight ? ThemeValues.accentCol : ThemeValues.primaryCol
                font.pixelSize: 10; font.family: ThemeValues.fontFamily; font.bold: true
                opacity: 0.7
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 3

                Text {
                    text: value
                    color: highlight ? ThemeValues.accentCol : ThemeValues.textCol
                    font.pixelSize: ThemeValues.fontSize + 10; font.family: ThemeValues.fontFamily; font.bold: true
                }
                Text {
                    text: unit
                    color: ThemeValues.textCol
                    font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily
                    opacity: 0.4
                    anchors.baseline: parent.children[0].baseline
                    visible: unit !== ""
                }
            }
        }
    }
}
