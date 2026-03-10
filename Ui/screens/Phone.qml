import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import HeadUnit

Item {
    id: root
    property var theme: null
    property var bluetoothManager: null
    signal callActive(bool isActive)


    property string phoneNumber: ""
    property bool isInCall: bluetoothManager ? bluetoothManager.hasActiveCall : false
    property string callStatus: getCallStatus()

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        // HORIZONTAL SPLIT LAYOUT
        Row {
            anchors.fill: parent
            spacing: 0

            // LEFT SIDE: DIALPAD ONLY (500px)
            Rectangle {
                width: 500
                height: parent.height
                color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.2)
                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                border.width: 1

                Column {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 0

                    // Dialpad Grid (3x4)
                    Grid {
                        width: parent.width
                        height: parent.height
                        columns: 3
                        rowSpacing: 6
                        columnSpacing: 6

                        Repeater {
                            model: [
                                { num: "1", let: "" },
                                { num: "2", let: "ABC" },
                                { num: "3", let: "DEF" },
                                { num: "4", let: "GHI" },
                                { num: "5", let: "JKL" },
                                { num: "6", let: "MNO" },
                                { num: "7", let: "PQRS" },
                                { num: "8", let: "TUV" },
                                { num: "9", let: "WXYZ" },
                                { num: "*", let: "" },
                                { num: "0", let: "+" },
                                { num: "#", let: "" }
                            ]

                            Rectangle {
                                width: (parent.width - 12) / 3
                                height: (parent.height - 18) / 4
                                color: dialMouseArea.pressed ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3) :
                                       dialMouseArea.containsMouse ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2) :
                                       Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.4)
                                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.6)
                                border.width: 1
                                radius: 6

                                Behavior on color { ColorAnimation { duration: 100 } }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 0

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.num
                                        color: ThemeValues.textCol
                                        font.pixelSize: 28
                                        font.family: ThemeValues.fontFamily
                                        font.weight: Font.Bold
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.let
                                        color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                                        font.pixelSize: 9
                                        font.family: ThemeValues.fontFamily
                                        visible: modelData.let !== ""
                                    }
                                }

                                MouseArea {
                                    id: dialMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        if (!isInCall) {
                                            phoneNumber += modelData.num
                                        }
                                    }
                                }
                            }
                        }
                    }

                }
            }

            // RIGHT SIDE: PHONE NUMBER DISPLAY & CALL BUTTONS
            Rectangle {
                width: parent.width - 500
                height: parent.height
                color: "transparent"

                Column {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    // Large Phone Number Display
                    Rectangle {
                        width: parent.width
                        height: 100
                        color: Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.5)
                        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.7)
                        border.width: 2
                        radius: 10

                        Column {
                            anchors.centerIn: parent
                            width: parent.width - 30
                            spacing: 5

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Phone Number"
                                color: Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.5)
                                font.pixelSize: 12
                                font.family: ThemeValues.fontFamily
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width
                                text: phoneNumber || "Enter number..."
                                color: phoneNumber ? ThemeValues.textCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                                font.pixelSize: 36
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                            }
                        }
                    }

                    // Backspace Button
                    Rectangle {
                        width: parent.width
                        height: 45
                        color: backspaceMouseArea.pressed ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3) :
                               backspaceMouseArea.containsMouse ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2) :
                               Qt.rgba(ThemeValues.cardBgCol.r, ThemeValues.cardBgCol.g, ThemeValues.cardBgCol.b, 0.4)
                        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.6)
                        border.width: 1
                        radius: 6
                        enabled: phoneNumber.length > 0

                        Behavior on color { ColorAnimation { duration: 100 } }

                        Text {
                            anchors.centerIn: parent
                            text: "⌫ Backspace"
                            color: phoneNumber.length > 0 ? ThemeValues.textCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                            font.pixelSize: 16
                            font.family: ThemeValues.fontFamily
                            font.weight: Font.Medium
                        }

                        MouseArea {
                            id: backspaceMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: phoneNumber.length > 0
                            onClicked: {
                                if (phoneNumber.length > 0) {
                                    phoneNumber = phoneNumber.slice(0, -1)
                                }
                            }
                        }
                    }

                    // CALL Button (when not in call)
                    Rectangle {
                        width: parent.width
                        height: 70
                        visible: !isInCall
                        color: callMouseArea.pressed ? Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.4) :
                               callMouseArea.containsMouse ? Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.3) :
                               Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.25)
                        border.color: phoneNumber.length > 0 ? Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.8) : Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.4)
                        border.width: 2
                        radius: 8
                        enabled: phoneNumber.length > 0

                        Behavior on color { ColorAnimation { duration: 100 } }

                        Row {
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                text: "📞"
                                font.pixelSize: 32
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "CALL"
                                color: phoneNumber.length > 0 ? ThemeValues.successCol : Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.4)
                                font.pixelSize: 28
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                            }
                        }

                        MouseArea {
                            id: callMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: phoneNumber.length > 0
                            onClicked: {
                                if (phoneNumber.length > 0) {
                                    console.log("Initiating call to:", phoneNumber)
                                    if (bluetoothManager) {
                                        bluetoothManager.dialNumber(phoneNumber)
                                    }
                                }
                            }
                        }
                    }

                    // END CALL Button (when in call)
                    Rectangle {
                        width: parent.width
                        height: 70
                        visible: isInCall
                        color: endCallMouseArea.pressed ? Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.4) :
                               endCallMouseArea.containsMouse ? Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.3) :
                               Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.25)
                        border.color: Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.8)
                        border.width: 2
                        radius: 8

                        Behavior on color { ColorAnimation { duration: 100 } }

                        Row {
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                text: "📵"
                                font.pixelSize: 32
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "END CALL"
                                color: ThemeValues.errorCol
                                font.pixelSize: 28
                                font.family: ThemeValues.fontFamily
                                font.weight: Font.Bold
                            }
                        }

                        MouseArea {
                            id: endCallMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                console.log("Ending call")
                                if (bluetoothManager) {
                                    bluetoothManager.hangupCall()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Helper function to get call status message
    function getCallStatus() {
        if (!bluetoothManager) return "No Bluetooth"

        if (bluetoothManager.hasActiveCall) {
            var state = bluetoothManager.activeCallState
            var number = bluetoothManager.activeCallNumber

            if (state === "incoming") {
                return "Incoming call from " + (number || "Unknown")
            } else if (state === "dialing" || state === "alerting") {
                return "Calling " + (number || "Unknown") + "..."
            } else if (state === "active") {
                return "In call with " + (number || "Unknown")
            } else {
                return "Call in progress"
            }
        }

        return "Ready"
    }
}
