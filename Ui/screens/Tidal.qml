import QtQuick 2.15
import "../" as Ui

Item {
    id: root
    property var theme: null

    signal messageFromJs(string cmd, var payload)

    Ui.TidalNative {
        anchors.fill: parent
        theme: root.theme

        onMessageFromJs: (cmd, payload) => {
            // Forward to Main.qml for activity tracking
            root.messageFromJs(cmd, payload)
        }
    }

    Component.onDestruction: {
        console.log("Tidal screen destroyed")
    }
}
