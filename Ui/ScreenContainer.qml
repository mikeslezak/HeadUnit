import QtQuick 2.15
import HeadUnit

Item {
    id: root

    property var theme: null
    property string currentScreen: "home"
    property string mapboxToken: ""
    property var appSettings: null

    signal screenSelected(string key)

    // Navigation state from Maps (exposed for GlancePanel)
    readonly property bool navActive: mapsLoader.item ? mapsLoader.item.routeActive : false
    readonly property string navManeuver: mapsLoader.item ? mapsLoader.item.nextManeuver : ""
    readonly property string navInstruction: mapsLoader.item ? mapsLoader.item.nextInstruction : ""
    readonly property string navStepDistance: mapsLoader.item ? mapsLoader.item.nextStepDistance : ""
    readonly property string navRouteDuration: mapsLoader.item ? mapsLoader.item.routeDuration : ""

    // Home screen IS the map — no separate Home.qml needed

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
        visible: root.currentScreen === "home"
        active: true
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

    Loader {
        id: appGridLoader
        anchors.fill: parent
        visible: root.currentScreen === "appgrid"
        active: false
        asynchronous: true
        source: "screens/AppGrid.qml"
        onLoaded: {
            item.theme = root.theme
            item.appSelected.connect(function(key) { root.screenSelected(key) })
        }
    }

    // Use Google Places for voice navigation (much better at resolving business names)
    Connections {
        target: placesSearchManager
        function onGeocodeCompleted(lat, lon, name) {
            console.log("ScreenContainer: Google geocode result:", name, "at", lat, lon)
            if (mapsLoader.item) {
                mapsLoader.item.getDirections(lat, lon, name)
            }
        }
    }

    function navigateTo(destination) {
        if (!mapsLoader.item) {
            console.warn("ScreenContainer: Maps not loaded, cannot navigate")
            return
        }
        // Use Google Places Text Search for accurate business/POI resolution
        placesSearchManager.geocodePlace(destination)
    }

    function show(key) {
        settingsLoader.active = (key === "settings")
        musicLoader.active = (key === "music")
        phoneLoader.active = (key === "phone")
        messagesLoader.active = (key === "messages")
        contactsLoader.active = (key === "contacts")
        weatherLoader.active = (key === "weather")
        vehicleLoader.active = (key === "vehicle")
        tuningLoader.active = (key === "tuning")
        tidalLoader.active = (key === "tidal")
        spotifyLoader.active = (key === "spotify")
        appGridLoader.active = (key === "appgrid")
    }
}
