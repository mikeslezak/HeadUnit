import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property var theme: null
    signal callActive(bool isActive)

    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color cardBgCol: theme?.palette?.cardBg ?? "#1a1a1f"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"

    property string phoneNumber: ""
    property bool isInCall: bluetoothManager ? bluetoothManager.hasActiveCall : false
    property string callStatus: getCallStatus()

    Rectangle {
        anchors.fill: parent
        color: bgCol

        // HORIZONTAL SPLIT LAYOUT
        Row {
            anchors.fill: parent
            spacing: 0

            // LEFT SIDE: DIALPAD ONLY (500px)
            Rectangle {
                width: 500
                height: parent.height
                color: Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.2)
                border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
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
                                color: dialMouseArea.pressed ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3) :
                                       dialMouseArea.containsMouse ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2) :
                                       Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.4)
                                border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.6)
                                border.width: 1
                                radius: 6

                                Behavior on color { ColorAnimation { duration: 100 } }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 0

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.num
                                        color: textCol
                                        font.pixelSize: 28
                                        font.family: fontFamily
                                        font.weight: Font.Bold
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: modelData.let
                                        color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                                        font.pixelSize: 9
                                        font.family: fontFamily
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
                        color: Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.5)
                        border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.7)
                        border.width: 2
                        radius: 10

                        Column {
                            anchors.centerIn: parent
                            width: parent.width - 30
                            spacing: 5

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Phone Number"
                                color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                                font.pixelSize: 12
                                font.family: fontFamily
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width
                                text: phoneNumber || "Enter number..."
                                color: phoneNumber ? textCol : Qt.rgba(textCol.r, textCol.g, textCol.b, 0.3)
                                font.pixelSize: 36
                                font.family: fontFamily
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
                        color: backspaceMouseArea.pressed ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3) :
                               backspaceMouseArea.containsMouse ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2) :
                               Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.4)
                        border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.6)
                        border.width: 1
                        radius: 6
                        enabled: phoneNumber.length > 0

                        Behavior on color { ColorAnimation { duration: 100 } }

                        Text {
                            anchors.centerIn: parent
                            text: "⌫ Backspace"
                            color: phoneNumber.length > 0 ? textCol : Qt.rgba(textCol.r, textCol.g, textCol.b, 0.3)
                            font.pixelSize: 16
                            font.family: fontFamily
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
                        color: callMouseArea.pressed ? Qt.rgba(0, 0.9, 0, 0.4) :
                               callMouseArea.containsMouse ? Qt.rgba(0, 0.9, 0, 0.3) :
                               Qt.rgba(0, 0.8, 0, 0.25)
                        border.color: phoneNumber.length > 0 ? Qt.rgba(0, 1, 0, 0.8) : Qt.rgba(0, 1, 0, 0.4)
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
                                color: phoneNumber.length > 0 ? "#00ff00" : Qt.rgba(0, 1, 0, 0.4)
                                font.pixelSize: 28
                                font.family: fontFamily
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
                        color: endCallMouseArea.pressed ? Qt.rgba(0.9, 0, 0, 0.4) :
                               endCallMouseArea.containsMouse ? Qt.rgba(0.9, 0, 0, 0.3) :
                               Qt.rgba(0.8, 0, 0, 0.25)
                        border.color: Qt.rgba(1, 0, 0, 0.8)
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
                                color: "#ff0000"
                                font.pixelSize: 28
                                font.family: fontFamily
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
