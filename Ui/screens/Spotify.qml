import QtQuick 2.15

Item {
    id: root
    property var theme: null
    signal messageFromJs(string cmd, var payload)

    Rectangle {
        anchors.fill: parent
        color: theme?.palette?.bg ?? "#0a0a0f"

        Column {
            anchors.centerIn: parent
            spacing: 20

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Spotify"
                color: "#1DB954"
                font.pixelSize: 32
                font.family: theme?.typography?.fontFamily ?? "Noto Sans"
                font.weight: Font.Bold
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Coming Soon"
                color: theme?.palette?.text ?? "white"
                font.pixelSize: 18
                font.family: theme?.typography?.fontFamily ?? "Noto Sans"
            }
        }
    }
}
