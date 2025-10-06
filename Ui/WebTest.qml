import QtQuick 2.15
import QtWebEngine

Item {
    id: root
    anchors.fill: parent

    WebEngineView {
        id: webView
        anchors.fill: parent
        settings.webGLEnabled: false
        settings.accelerated2dCanvasEnabled: false
        url: "qrc:/qt/qml/HeadUnit/assets/html/tidal_player.html"

        onLoadingChanged: (loadRequest) => {
            if (loadRequest.status === WebEngineView.LoadStarted) {
                console.log("WebTest: Loading started")
            } else if (loadRequest.status === WebEngineView.LoadSucceeded) {
                console.log("WebTest: Page loaded successfully")
            } else if (loadRequest.status === WebEngineView.LoadFailed) {
                console.error("WebTest: Load failed:", loadRequest.errorString)
            }
        }

        onJavaScriptConsoleMessage: (level, message, lineNumber, sourceID) => {
            console.log("WebTest JS:", message, "(line", lineNumber + ")")
        }
    }
}
