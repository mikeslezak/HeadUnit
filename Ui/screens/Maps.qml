import QtQuick 2.15
import "../" as Ui

Item {
    id: root
    property var theme: null

    // Expose activity signal to Main.qml
    signal messageFromJs(string cmd, var payload)

    property string googleMapsApiKey: "AIzaSyDaGnFfsUGjPhScHHGSuBetSpdR6oNgCs0"

    readonly property color textCol: theme?.palette?.text ?? "white"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"

    Ui.MapWeb {
        id: mapView
        anchors.fill: parent
        theme: root.theme

        onReadyChanged: {
            if (mapView.ready) {
                mapView.sendToJs("init", {
                    lat: 51.0447,
                    lng: -114.0719,
                    apiKey: root.googleMapsApiKey
                })
            }
        }

        onMessageFromJs: (cmd, payload) => {
            console.log("Maps:", cmd, JSON.stringify(payload))

            // Forward to Main.qml for activity tracking
            root.messageFromJs(cmd, payload)

            // Show API warning if needed
            if (cmd === "mapReady") {
                apiKeyWarning.visible = (root.googleMapsApiKey === "YOUR_API_KEY_HERE")
            }
        }
    }

    // Minimal API Key warning - only shows if needed
    Rectangle {
        id: apiKeyWarning
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            topMargin: 40
        }
        height: 100
        color: Qt.rgba(0, 0, 0, 0.95)
        border.color: root.primaryCol
        border.width: 2
        z: 200
        visible: false

        Column {
            anchors.centerIn: parent
            spacing: 10
            width: parent.width - 40

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Text {
                    text: "⚠️"
                    color: root.primaryCol
                    font.pixelSize: 20
                }

                Text {
                    text: "Google Maps API Key Required"
                    color: root.primaryCol
                    font.pixelSize: 16
                    font.family: root.fontFamily
                    font.weight: Font.Bold
                }
            }

            Text {
                width: parent.width
                text: "Get a FREE API key at: console.cloud.google.com/google/maps-apis\nThen add it to Ui/screens/Maps.qml (line 9: googleMapsApiKey property)"
                color: root.textCol
                font.pixelSize: 11
                font.family: root.fontFamily
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.4
                opacity: 0.9
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Map will work with 'For development purposes only' watermark without key"
                color: root.textCol
                font.pixelSize: 10
                font.family: root.fontFamily
                opacity: 0.6
                font.italic: true
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: apiKeyWarning.visible = false
        }

        // Auto-hide after 8 seconds
        Timer {
            running: apiKeyWarning.visible
            interval: 8000
            onTriggered: apiKeyWarning.visible = false
        }
    }

    Component.onDestruction: {
        console.log("Maps screen destroyed")
    }
}
