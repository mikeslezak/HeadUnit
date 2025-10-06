import QtQuick 2.15

FontLoader {
    id: root

    onStatusChanged: {
        if (status === FontLoader.Ready) {
            console.log("Font ready:", name)
        } else if (status === FontLoader.Error) {
            console.error("Font failed to load:", source)
        } else if (status === FontLoader.Loading) {
            console.log("Font loading:", source)
        }
    }
}
