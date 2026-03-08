import QtQuick 2.15
import QtWebEngine 1.9

Item {
    id: root
    property var theme: null

    readonly property color textCol: theme?.palette?.text ?? "white"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"

    WebEngineView {
        id: webView
        anchors.fill: parent

        // Use mobile user agent to get mobile Google Maps interface
        profile.httpUserAgent: "Mozilla/5.0 (Linux; Android 13; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36"

        // Load Google Maps mobile web app directly
        url: "https://www.google.com/maps/@51.0447,-114.0719,13z"

        settings.localContentCanAccessRemoteUrls: true
        settings.javascriptEnabled: true
        settings.pluginsEnabled: false

        // Enable geolocation - this allows Google Maps to request location
        onFeaturePermissionRequested: function(securityOrigin, feature) {
            if (feature === WebEngineView.Geolocation) {
                console.log("Geolocation permission requested by:", securityOrigin)
                webView.grantFeaturePermission(securityOrigin, feature, true)
                console.log("Geolocation permission granted")
            }
        }

        onLoadingChanged: function(loadRequest) {
            if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                console.log("Maps loaded successfully")
            } else if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                console.error("Maps load failed:", loadRequest.errorString)
            }
        }
    }

    // GPS integration will be added when USB GPS dongle is connected

    // Loading indicator
    Rectangle {
        anchors.fill: parent
        color: root.bgCol
        visible: webView.loading
        z: 100

        Column {
            anchors.centerIn: parent
            spacing: 16

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Loading Maps..."
                color: root.textCol
                font.pixelSize: 20
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 200
                height: 4
                color: "transparent"
                border.color: root.primaryCol
                border.width: 1
                radius: 2

                Rectangle {
                    width: parent.width * 0.3
                    height: parent.height
                    color: root.primaryCol
                    radius: 2

                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation { from: 0; to: 140; duration: 1500; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 140; to: 0; duration: 1500; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }
    }

    Component.onDestruction: {
        console.log("Maps screen destroyed")
        webView.stop()
        webView.url = "about:blank"
    }
}
