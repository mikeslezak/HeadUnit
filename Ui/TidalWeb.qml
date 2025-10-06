import QtQuick 2.15
import QtWebEngine 1.9
import QtWebChannel 1.15

Item {
  id: root
  anchors.fill: parent
  required property var theme
  signal messageFromJs(string cmd, var payload)

  property bool ready: false
  property bool initialized: false

  // QML-side bridge object for WebChannel
  QtObject {
    id: bridge
    WebChannel.id: "qmlBridge"

    // Called from JavaScript
    function sendToQml(cmd, payloadJson) {
      try {
        const payload = payloadJson ? JSON.parse(payloadJson) : {}
        console.log("Bridge received:", cmd, payloadJson)

        // Set ready when WebChannel connects
        if (cmd === "channelReady" && !root.ready) {
          root.ready = true
          console.log("TidalWeb is now ready for commands")
        }

        root.messageFromJs(cmd, payload)
      } catch (e) {
        console.error("Bridge parse error:", e)
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
    color: "#00000080"
    visible: !root.ready
    z: 50
    Text {
      anchors.centerIn: parent
      text: "Loading…"
      color: "white"
      font.pixelSize: 20
    }
  }

  WebEngineView {
    id: web
    anchors.fill: parent
    url: Qt.resolvedUrl("qrc:/qt/qml/HeadUnit/assets/html/tidal_player.html")
    webChannel: channel

    settings.localContentCanAccessRemoteUrls: true
    settings.javascriptEnabled: true

    onJavaScriptConsoleMessage: (level, msg, line, source) => {
      console.log(`JS Console [${level}]: ${msg} (line ${line}, ${source})`)
    }

    onLoadingChanged: (req) => {
      if (req.status === WebEngineView.LoadSucceeded) {
        console.log("TidalWeb loaded successfully")
        root.ready = true
        root.messageFromJs("ready", {})
      } else if (req.status === WebEngineView.LoadFailed) {
        console.error("TidalWeb load failed")
      }
    }
  }

  // Send commands from QML to JavaScript
  function sendToJs(cmd, payload) {
    if (!root.ready) {
      console.warn("Can't send to JS - not ready yet")
      return
    }

    if (!web) {
      console.warn("WebEngineView not available")
      return
    }

    const payloadStr = JSON.stringify(payload || {})
    const script = `
      if (window.tidalHandler) {
        window.tidalHandler.handleCommand('${cmd}', ${payloadStr});
      } else {
        console.error('tidalHandler not initialized');
      }
    `
    web.runJavaScript(script)
  }

  // Function to trigger unlock from QML
  function unlockPlayback() {
    sendToJs("unlock", {})
  }

  Component.onDestruction: {
    console.log("TidalWeb destroying...")
    if (web) {
      web.stop()
      web.url = "about:blank"
    }
  }
}
