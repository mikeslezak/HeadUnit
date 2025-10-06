import QtQuick 2.15
import QtWebEngine 1.9
import QtWebChannel 1.15

Item {
    id: root
    anchors.fill: parent
    required property var theme
    signal messageFromJs(string cmd, var payload)

    property bool ready: false

    // QML-side bridge object for WebChannel
    QtObject {
        id: bridge
        WebChannel.id: "qmlBridge"

        function sendToQml(cmd, payloadJson) {
            try {
                const payload = payloadJson ? JSON.parse(payloadJson) : {}
                console.log("MapWeb received:", cmd, payloadJson)

                if (cmd === "channelReady" && !root.ready) {
                    root.ready = true
                    console.log("MapWeb is now ready")
                    // Apply initial theme
                    Qt.callLater(applyTheme)
                }

                root.messageFromJs(cmd, payload)
            } catch (e) {
                console.error("MapWeb bridge parse error:", e)
            }
        }
    }

    WebChannel {
        id: channel
        registeredObjects: [bridge]
    }

    // Loading overlay
    Rectangle {
        anchors.fill: parent
        color: theme?.palette?.bg ?? "#0a0a0f"
        visible: !root.ready
        z: 50

        Column {
            anchors.centerIn: parent
            spacing: 16

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Loading Map..."
                color: theme?.palette?.text ?? "white"
                font.pixelSize: 20
                font.family: theme?.typography?.fontFamily ?? "Noto Sans"
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 200
                height: 4
                color: "transparent"
                border.color: theme?.palette?.primary ?? "#00f0ff"
                border.width: 1
                radius: 2

                Rectangle {
                    id: loadingBar
                    width: parent.width * 0.3
                    height: parent.height
                    color: theme?.palette?.primary ?? "#00f0ff"
                    radius: 2

                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation {
                            from: 0
                            to: 200 - 60
                            duration: 1500
                            easing.type: Easing.InOutQuad
                        }
                        NumberAnimation {
                            from: 200 - 60
                            to: 0
                            duration: 1500
                            easing.type: Easing.InOutQuad
                        }
                    }
                }
            }
        }
    }

    WebEngineView {
        id: web
        anchors.fill: parent
        url: Qt.resolvedUrl("qrc:/qt/qml/HeadUnit/assets/html/map_view.html")
        webChannel: channel

        settings.localContentCanAccessRemoteUrls: true
        settings.javascriptEnabled: true

        onJavaScriptConsoleMessage: (level, msg, line, source) => {
            console.log(`Map JS [${level}]: ${msg} (line ${line})`)
        }

        onLoadingChanged: (req) => {
            if (req.status === WebEngineView.LoadSucceeded) {
                console.log("MapWeb loaded successfully")
            } else if (req.status === WebEngineView.LoadFailed) {
                console.error("MapWeb load failed:", req.errorString)
            }
        }
    }

    // Send commands from QML to JavaScript
    function sendToJs(cmd, payload) {
        if (!root.ready) {
            console.warn("MapWeb not ready yet")
            return
        }

        const payloadStr = JSON.stringify(payload || {})
        const script = `
            if (window.mapHandler) {
                window.mapHandler.handleCommand('${cmd}', ${payloadStr});
            } else {
                console.error('mapHandler not initialized');
            }
        `
        web.runJavaScript(script)
    }

    // Apply current theme to the map
    function applyTheme() {
        if (!root.ready || !theme) return

        const themeData = {
            bg: theme.palette?.bg ?? "#0a0a0f",
            primary: theme.palette?.primary ?? "#00f0ff",
            text: theme.palette?.text ?? "#39ff14"
        }

        console.log("Applying theme to map:", JSON.stringify(themeData))
        sendToJs("applyTheme", themeData)
    }

    // Watch for theme changes
    Connections {
        target: theme
        function onThemeChanged() {
            console.log("MapWeb: Theme changed, reapplying")
            Qt.callLater(root.applyTheme)
        }
    }

    Component.onDestruction: {
        console.log("MapWeb destroying...")
        if (web) {
            web.stop()
            web.url = "about:blank"
        }
    }
}
