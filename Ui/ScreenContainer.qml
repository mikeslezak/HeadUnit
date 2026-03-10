import QtQuick 2.15
import HeadUnit

Item {
    id: root

    property var theme: null
    property string currentScreen: "home"
    property string mapboxToken: ""
    property var appSettings: null

    signal screenSelected(string key)

    Loader {
        id: homeLoader
        anchors.fill: parent
        visible: root.currentScreen === "home"
        active: true
        asynchronous: true
        source: "screens/Home.qml"
        onLoaded: {
            item.theme = root.theme
            item.mapboxToken = Qt.binding(function() { return root.mapboxToken })
            if (item.appSelected) {
                item.appSelected.connect(function(key) { root.screenSelected(key) })
            }
        }
    }

    Loader {
        id: settingsLoader
        anchors.fill: parent
        visible: root.currentScreen === "settings"
        active: false
        asynchronous: true
        source: "screens/Settings.qml"
        onLoaded: {
            item.theme = root.theme
            item.appSettings = root.appSettings
        }
    }

    Loader {
        id: musicLoader
        anchors.fill: parent
        visible: root.currentScreen === "music"
        active: false
        asynchronous: true
        source: "screens/Music.qml"
        onLoaded: {
            item.theme = root.theme
            item.bluetoothManager = Qt.binding(function() { return bluetoothManager })
            if (item.messageFromJs) {
                item.messageFromJs.connect(function(msg) {
                    console.log("Music screen message:", msg)
                })
            }
        }
    }

    Loader {
        id: mapsLoader
        anchors.fill: parent
        visible: root.currentScreen === "maps"
        active: false
        asynchronous: true
        source: "screens/Maps.qml"
        onLoaded: {
            item.theme = root.theme
            item.mapboxToken = Qt.binding(function() { return root.mapboxToken })
            console.log("Maps loaded, token:", root.mapboxToken ? root.mapboxToken.substring(0, 10) + "..." : "EMPTY")
        }
    }

    Loader {
        id: phoneLoader
        anchors.fill: parent
        visible: root.currentScreen === "phone"
        active: false
        asynchronous: true
        source: "screens/Phone.qml"
        onLoaded: {
            item.theme = root.theme
            item.bluetoothManager = Qt.binding(function() { return bluetoothManager })
        }
    }

    Loader {
        id: messagesLoader
        anchors.fill: parent
        visible: root.currentScreen === "messages"
        active: false
        asynchronous: true
        source: "screens/Messages.qml"
        onLoaded: {
            item.theme = root.theme
            item.bluetoothManager = Qt.binding(function() { return bluetoothManager })
            item.messageManager = Qt.binding(function() { return messageManager })
        }
    }

    Loader {
        id: contactsLoader
        anchors.fill: parent
        visible: root.currentScreen === "contacts"
        active: false
        asynchronous: true
        source: "screens/Contacts.qml"
        onLoaded: {
            item.theme = root.theme
            item.bluetoothManager = Qt.binding(function() { return bluetoothManager })
            item.contactManager = Qt.binding(function() { return contactManager })
        }
    }

    Loader {
        id: weatherLoader
        anchors.fill: parent
        visible: root.currentScreen === "weather"
        active: false
        asynchronous: true
        source: "screens/Weather.qml"
        onLoaded: { item.theme = root.theme }
    }

    Loader {
        id: vehicleLoader
        anchors.fill: parent
        visible: root.currentScreen === "vehicle"
        active: false
        asynchronous: true
        source: "screens/Vehicle.qml"
        onLoaded: { item.theme = root.theme }
    }

    Loader {
        id: tuningLoader
        anchors.fill: parent
        visible: root.currentScreen === "tuning"
        active: false
        asynchronous: true
        source: "screens/Tuning.qml"
        onLoaded: { item.theme = root.theme }
    }

    Loader {
        id: tidalLoader
        anchors.fill: parent
        visible: root.currentScreen === "tidal"
        active: false
        asynchronous: true
        source: "screens/Tidal.qml"
        onLoaded: { item.theme = root.theme }
    }

    Loader {
        id: spotifyLoader
        anchors.fill: parent
        visible: root.currentScreen === "spotify"
        active: false
        asynchronous: true
        source: "screens/Spotify.qml"
        onLoaded: { item.theme = root.theme }
    }

    function show(key) {
        homeLoader.active = (key === "home")
        settingsLoader.active = (key === "settings")
        musicLoader.active = (key === "music")
        mapsLoader.active = (key === "maps")
        phoneLoader.active = (key === "phone")
        messagesLoader.active = (key === "messages")
        contactsLoader.active = (key === "contacts")
        weatherLoader.active = (key === "weather")
        vehicleLoader.active = (key === "vehicle")
        tuningLoader.active = (key === "tuning")
        tidalLoader.active = (key === "tidal")
        spotifyLoader.active = (key === "spotify")
    }
}
