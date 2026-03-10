import QtQuick 2.15
import HeadUnit

Item {
    id: root
    property var theme: null
    property string mapboxToken: ""

    signal appSelected(string key)

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        // Dark map-like background (static — saves ~65% CPU vs live MapView)
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(0.06, 0.06, 0.10, 1.0) }
                GradientStop { position: 0.5; color: Qt.rgba(0.08, 0.08, 0.12, 1.0) }
                GradientStop { position: 1.0; color: Qt.rgba(0.05, 0.05, 0.08, 1.0) }
            }

            // Subtle grid lines for map feel
            Canvas {
                anchors.fill: parent
                opacity: 0.06
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.strokeStyle = ThemeValues.primaryCol.toString()
                    ctx.lineWidth = 0.5
                    var spacing = 60
                    for (var x = 0; x < width; x += spacing) {
                        ctx.beginPath()
                        ctx.moveTo(x, 0)
                        ctx.lineTo(x, height)
                        ctx.stroke()
                    }
                    for (var y = 0; y < height; y += spacing) {
                        ctx.beginPath()
                        ctx.moveTo(0, y)
                        ctx.lineTo(width, y)
                        ctx.stroke()
                    }
                }
            }
        }

        // "Tap to open Maps" hint
        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "\uD83D\uDDFA"
                font.pixelSize: 48
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Tap to open Maps"
                color: ThemeValues.textCol
                font.pixelSize: ThemeValues.fontSize
                font.family: ThemeValues.fontFamily
                opacity: 0.4
            }
        }

        // Tap to go to Maps screen
        MouseArea {
            anchors.fill: parent
            onClicked: root.appSelected("maps")
        }
    }
}
