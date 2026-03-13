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
                GradientStop { position: 0.0; color: Qt.darker(ThemeValues.bgCol, 1.15) }
                GradientStop { position: 0.5; color: Qt.darker(ThemeValues.bgCol, 1.05) }
                GradientStop { position: 1.0; color: Qt.darker(ThemeValues.bgCol, 1.2) }
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

            Canvas {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 48; height: 48
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = ThemeValues.primaryCol.toString()
                    ctx.fillStyle = ThemeValues.primaryCol.toString()
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    // Map pin
                    ctx.beginPath()
                    ctx.arc(24, 18, 10, Math.PI, 0)
                    ctx.lineTo(24, 40)
                    ctx.closePath()
                    ctx.stroke()
                    // Inner dot
                    ctx.beginPath()
                    ctx.arc(24, 18, 4, 0, Math.PI * 2)
                    ctx.fill()
                }
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
