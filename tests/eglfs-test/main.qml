import QtQuick

Window {
    id: root
    visible: true
    width: 932
    height: 430
    color: "#0a0a12"
    title: "HeadUnit EGLFS Test"

    // Gradient background
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0a0a12" }
            GradientStop { position: 1.0; color: "#1a1a2e" }
        }
    }

    // Center content
    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "HEADUNIT"
            font.pixelSize: 64
            font.bold: true
            color: "#00ffcc"

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { to: 0.4; duration: 1500; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 1500; easing.type: Easing.InOutSine }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "EGLFS Display Test"
            font.pixelSize: 24
            color: "#667788"
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Phase 2 Verified"
            font.pixelSize: 18
            color: "#445566"
        }
    }

    // Corner markers to verify full screen coverage
    Rectangle { x: 0; y: 0; width: 20; height: 20; color: "#ff0044" }
    Rectangle { x: parent.width - 20; y: 0; width: 20; height: 20; color: "#00ff44" }
    Rectangle { x: 0; y: parent.height - 20; width: 20; height: 20; color: "#0044ff" }
    Rectangle { x: parent.width - 20; y: parent.height - 20; width: 20; height: 20; color: "#ffff00" }

    // FPS counter
    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 30
        text: "Frame: " + frameCounter
        font.pixelSize: 14
        color: "#334455"

        property int frameCounter: 0
        NumberAnimation on frameCounter {
            from: 0; to: 999999
            duration: 999999 * 16
        }
    }
}
