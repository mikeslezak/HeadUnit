import QtQuick 2.15
import QtLocation 6.5
import QtPositioning 6.5
import Qt.labs.settings 1.0
import MapLibre 3.0
import HeadUnit

Item {
    id: root
    property var theme: null
    property string mapboxToken: ""

    property bool mapReady: false
    property bool followMode: true
    property string activeStyleName: "Dark"
    property bool searchResultsVisible: false
    property var searchResults: []

    // Persist map viewport and style (no 'category' — not supported in Qt 6.5)
    Settings {
        id: mapPrefs
        property double mapSavedLat: 50.7358
        property double mapSavedLon: -113.966
        property double mapSavedZoom: 12
        property string mapSavedStyle: "Dark"
    }

    // Route state
    property bool routeActive: false
    property string routeDistance: ""
    property string routeDuration: ""
    property var routeDestination: null   // {lat, lon, name}
    property var routeGeoJson: ({})       // GeoJSON for the route line

    // Weather overlay
    property bool radarVisible: false
    property string _owmKey: (typeof owmApiKey !== 'undefined') ? owmApiKey : ""
    property var _weatherLayers: []   // array of {sourceParam, layerParam} objects
    property var _mapStyle: mapLoader.item ? mapLoader.item._styleRef : null
    property real _preRadarZoom: -1   // saved zoom before radar zoom-out
    property var _preRadarCenter: null // saved center before radar zoom-out

    // Turn-by-turn state
    property var routeSteps: []           // Array of step objects from Directions API
    property int currentStep: 0           // Index into routeSteps
    property string nextManeuver: ""      // e.g. "turn-right", "arrive"
    property string nextInstruction: ""   // Human-readable instruction
    property string nextStepDistance: ""  // Distance to next maneuver

    // Save map viewport when user stops panning
    Timer {
        id: saveViewportTimer
        interval: 2000
        onTriggered: {
            if (mapLoader.item) {
                var c = mapLoader.item.map.center
                mapPrefs.mapSavedLat = c.latitude
                mapPrefs.mapSavedLon = c.longitude
                mapPrefs.mapSavedZoom = mapLoader.item.map.zoomLevel
            }
        }
    }

    // OWM weather layer definitions: [id, layerName, opacity]
    readonly property var _owmLayerDefs: [
        ["precipitation", "precipitation_new", 1.0]
    ]

    function toggleRadar() {
        if (!root._mapStyle) {
            console.warn("Weather: map style not available")
            return
        }

        if (root.radarVisible) {
            removeWeatherLayers()
            root.radarVisible = false

            // Snap back to pre-radar view if we zoomed out
            if (root._preRadarZoom > 0 && mapLoader.item) {
                mapLoader.item.map.zoomLevel = root._preRadarZoom
                if (root._preRadarCenter)
                    mapLoader.item.map.center = root._preRadarCenter
                root._preRadarZoom = -1
                root._preRadarCenter = null
                followMode = true
            }
            console.log("Weather overlays removed")
        } else {
            root.radarVisible = true

            // Auto zoom-out during navigation so weather layers are visible
            if (root.routeActive && mapLoader.item && mapLoader.item.map.zoomLevel > 8) {
                root._preRadarZoom = mapLoader.item.map.zoomLevel
                root._preRadarCenter = mapLoader.item.map.center
                mapLoader.item.map.zoomLevel = 7
                followMode = false
                if (root.routeDestination && gps.position.coordinateValid) {
                    var midLat = (gps.position.coordinate.latitude + root.routeDestination.lat) / 2
                    var midLon = (gps.position.coordinate.longitude + root.routeDestination.lon) / 2
                    mapLoader.item.map.center = QtPositioning.coordinate(midLat, midLon)
                }
            }

            addWeatherLayers()
        }
    }

    function removeWeatherLayers() {
        for (var i = root._weatherLayers.length - 1; i >= 0; i--) {
            var entry = root._weatherLayers[i]
            if (entry.layerParam && root._mapStyle) {
                root._mapStyle.removeParameter(entry.layerParam)
                entry.layerParam.destroy()
            }
            if (entry.sourceParam && root._mapStyle) {
                root._mapStyle.removeParameter(entry.sourceParam)
                entry.sourceParam.destroy()
            }
        }
        root._weatherLayers = []
    }

    function addWeatherLayers() {
        if (!root._mapStyle) return

        removeWeatherLayers()

        if (root._owmKey !== "") {
            // Add all OWM layers
            var layers = []
            for (var i = 0; i < root._owmLayerDefs.length; i++) {
                var def = root._owmLayerDefs[i]
                var id = def[0]
                var layerName = def[1]
                var opacity = def[2]
                var tileUrl = "https://tile.openweathermap.org/map/" + layerName + "/{z}/{x}/{y}.png?appid=" + root._owmKey

                var srcParam = Qt.createQmlObject(
                    'import MapLibre 3.0; SourceParameter { styleId: "wx-' + id + '-src"; type: "raster"; property var tiles: ["' + tileUrl + '"]; property int tileSize: 256 }',
                    root._mapStyle, "wxSrc" + id)
                root._mapStyle.addParameter(srcParam)

                var layerParam = Qt.createQmlObject(
                    'import MapLibre 3.0; LayerParameter { styleId: "wx-' + id + '-layer"; type: "raster"; property string source: "wx-' + id + '-src"; paint: ({"raster-opacity": ' + opacity + '}) }',
                    root._mapStyle, "wxLayer" + id)
                root._mapStyle.addParameter(layerParam)

                layers.push({ sourceParam: srcParam, layerParam: layerParam })
                console.log("Weather layer added:", id, "opacity:", opacity)
            }
            root._weatherLayers = layers
        } else {
            // Fallback: RainViewer precipitation only
            fetchRainViewerUrl()
        }
    }

    function fetchRainViewerUrl() {
        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                try {
                    var data = JSON.parse(xhr.responseText)
                    var radar = data.radar
                    if (radar && radar.past && radar.past.length > 0) {
                        var latest = radar.past[radar.past.length - 1]
                        var tileUrl = "https://tilecache.rainviewer.com" + latest.path + "/256/{z}/{x}/{y}/2/1_1.png"

                        var srcParam = Qt.createQmlObject(
                            'import MapLibre 3.0; SourceParameter { styleId: "wx-precip-src"; type: "raster"; property var tiles: ["' + tileUrl + '"]; property int tileSize: 256 }',
                            root._mapStyle, "wxSrcPrecip")
                        root._mapStyle.addParameter(srcParam)

                        var layerParam = Qt.createQmlObject(
                            'import MapLibre 3.0; LayerParameter { styleId: "wx-precip-layer"; type: "raster"; property string source: "wx-precip-src"; paint: ({"raster-opacity": 0.8}) }',
                            root._mapStyle, "wxLayerPrecip")
                        root._mapStyle.addParameter(layerParam)

                        root._weatherLayers = [{ sourceParam: srcParam, layerParam: layerParam }]
                        console.log("Weather: RainViewer fallback layer added")
                    }
                } catch (e) {
                    console.error("RainViewer API error:", e)
                }
            }
        }
        xhr.open("GET", "https://api.rainviewer.com/public/weather-maps.json")
        xhr.send()
    }

    Component.onDestruction: {
        if (mapLoader.item) {
            var c = mapLoader.item.map.center
            mapPrefs.mapSavedLat = c.latitude
            mapPrefs.mapSavedLon = c.longitude
            mapPrefs.mapSavedZoom = mapLoader.item.map.zoomLevel
        }
    }

    // GPS position source
    PositionSource {
        id: gps
        updateInterval: 1000
        active: true

        onPositionChanged: {
            if (position.coordinateValid && followMode && mapLoader.item) {
                mapLoader.item.map.center = position.coordinate
            }
            // Forward GPS to ContextAggregator for voice assistant location awareness
            if (position.coordinateValid && typeof contextAggregator !== 'undefined') {
                contextAggregator.gpsLatitude = position.coordinate.latitude
                contextAggregator.gpsLongitude = position.coordinate.longitude
                if (position.speedValid) {
                    contextAggregator.gpsSpeed = position.speed * 3.6 // m/s to km/h
                }
            }
            // Forward GPS to SpeedLimitManager for live speed limit tracking
            if (position.coordinateValid && typeof speedLimitManager !== 'undefined') {
                speedLimitManager.updateGpsPosition(
                    position.coordinate.latitude,
                    position.coordinate.longitude,
                    position.speedValid ? position.speed * 3.6 : 0)
            }
            if (position.coordinateValid && routeActive) {
                advanceStepIfNeeded()
            }
        }
    }

    // Defer map creation until token is available
    Loader {
        id: mapLoader
        anchors.fill: parent
        active: root.mapboxToken !== ""
        sourceComponent: mapComponent

        onLoaded: {
            root.mapReady = true

            // Restore saved style
            selectStyle(mapPrefs.mapSavedStyle || "Dark")

            // Restore saved position (GPS overrides if available)
            if (gps.position.coordinateValid) {
                item.map.center = gps.position.coordinate
                item.map.zoomLevel = 14
            } else {
                item.map.center = QtPositioning.coordinate(mapPrefs.mapSavedLat, mapPrefs.mapSavedLon)
                item.map.zoomLevel = mapPrefs.mapSavedZoom
            }
        }
    }

    function selectStyle(styleName) {
        if (!mapLoader.item) return
        var types = mapLoader.item.map.supportedMapTypes
        for (var i = 0; i < types.length; i++) {
            if (types[i].name === styleName) {
                mapLoader.item.map.activeMapType = types[i]
                activeStyleName = styleName
                mapPrefs.mapSavedStyle = styleName
                return
            }
        }
    }

    Component {
        id: mapComponent

        MapView {
            id: mapView
            property alias _styleRef: mapStyle

            map.plugin: Plugin {
                name: "maplibre"

                PluginParameter {
                    name: "maplibre.api.provider"
                    value: "mapbox"
                }
                PluginParameter {
                    name: "maplibre.api.key"
                    value: root.mapboxToken
                }
            }

            map.center: QtPositioning.coordinate(50.7358, -113.966)
            map.zoomLevel: 12

            Connections {
                target: mapView.map
                function onCenterChanged() { saveViewportTimer.restart() }
                function onZoomLevelChanged() { saveViewportTimer.restart() }
            }

            // ── MapLibre 3.0 Custom Layers ──
            MapLibre.style: Style {
                id: mapStyle

                // Route line source (GeoJSON)
                SourceParameter {
                    id: routeSource
                    styleId: "route-source"
                    type: "geojson"
                    property var data: root.routeGeoJson
                }

                // Route line casing (wider, darker line behind)
                LayerParameter {
                    styleId: "route-casing"
                    type: "line"
                    property string source: "route-source"

                    layout: ({
                        "line-join": "round",
                        "line-cap": "round"
                    })
                    paint: ({
                        "line-color": Qt.rgba(0, 0, 0, 0.4).toString(),
                        "line-width": 10
                    })
                }

                // Route line (primary color)
                LayerParameter {
                    styleId: "route-line"
                    type: "line"
                    property string source: "route-source"

                    layout: ({
                        "line-join": "round",
                        "line-cap": "round"
                    })
                    paint: ({
                        "line-color": ThemeValues.primaryCol.toString(),
                        "line-width": 5,
                        "line-opacity": 0.85
                    })
                }

            }

            // GPS location — outer ring (MapCircle is natively supported by MapLibre)
            MapCircle {
                id: gpsRing
                center: gps.position.coordinateValid
                    ? gps.position.coordinate
                    : QtPositioning.coordinate(0, 0)
                visible: gps.position.coordinateValid
                radius: 24
                color: "transparent"
                border.color: ThemeValues.primaryCol
                border.width: 2
                opacity: 0.4
            }

            // GPS location — inner dot
            MapCircle {
                id: gpsDot
                center: gps.position.coordinateValid
                    ? gps.position.coordinate
                    : QtPositioning.coordinate(0, 0)
                visible: gps.position.coordinateValid
                radius: 10
                color: ThemeValues.primaryCol
                border.color: ThemeValues.textCol
                border.width: 2
            }
        }
    }

    // ── Overlay markers (positioned on top of MapView via fromCoordinate) ──

    // Helper: reposition overlay markers when map moves or zooms
    property var _searchCoord: null
    property var _destCoord: null

    Timer {
        id: markerUpdateTimer
        interval: 16
        repeat: false
        onTriggered: root.updateOverlayMarkers()
    }

    Connections {
        target: mapLoader.item ? mapLoader.item.map : null
        function onCenterChanged() { markerUpdateTimer.restart() }
        function onZoomLevelChanged() { markerUpdateTimer.restart() }
    }

    function updateOverlayMarkers() {
        if (!mapLoader.item) return
        var map = mapLoader.item.map

        if (_searchCoord && searchOverlay.visible) {
            var sp = map.fromCoordinate(_searchCoord, false)
            searchOverlay.x = mapLoader.x + sp.x - searchOverlay.width / 2
            searchOverlay.y = mapLoader.y + sp.y - searchOverlay.height
        }

        if (_destCoord && destOverlay.visible) {
            var dp = map.fromCoordinate(_destCoord, false)
            destOverlay.x = mapLoader.x + dp.x - 2
            destOverlay.y = mapLoader.y + dp.y - destOverlay.height
        }
    }

    // Search result marker overlay
    Item {
        id: searchOverlay
        width: 24; height: 36
        visible: false
        z: 15

        Rectangle {
            width: 24; height: 24; radius: 12
            color: ThemeValues.accentCol
            anchors.top: parent.top

            Text {
                anchors.centerIn: parent
                text: "\u2022"
                color: ThemeValues.textCol
                font.pixelSize: 16; font.bold: true
            }
        }
        Canvas {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: 12; height: 14
            onPaint: {
                var ctx = getContext("2d")
                ctx.fillStyle = ThemeValues.accentCol.toString()
                ctx.beginPath()
                ctx.moveTo(0, 0)
                ctx.lineTo(12, 0)
                ctx.lineTo(6, 14)
                ctx.closePath()
                ctx.fill()
            }
        }
    }

    // Destination marker overlay
    Item {
        id: destOverlay
        width: 32; height: 56
        visible: root.routeActive && root.routeDestination !== null
        z: 15

        onVisibleChanged: if (visible) markerUpdateTimer.restart()

        Rectangle {
            x: 1; y: 0
            width: 2; height: 56
            color: ThemeValues.primaryCol
        }

        Rectangle {
            x: 3; y: 0
            width: 28; height: 20
            radius: 3
            color: ThemeValues.primaryCol

            Text {
                anchors.centerIn: parent
                text: "B"
                color: ThemeValues.bgCol
                font.pixelSize: 12
                font.family: ThemeValues.fontFamily
                font.bold: true
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.left
            anchors.horizontalCenterOffset: 2
            width: 8; height: 8; radius: 4
            color: ThemeValues.primaryCol
        }
    }

    // ── Search Bar ──
    Rectangle {
        id: searchBar
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        height: 48
        radius: 24
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.85)
        border.color: searchInput.activeFocus
            ? ThemeValues.primaryCol
            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
        border.width: 1
        z: 20

        Behavior on border.color { ColorAnimation { duration: 200 } }

        Row {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "\uD83D\uDD0D"
                font.pixelSize: 14
                opacity: 0.6
            }

            TextInput {
                id: searchInput
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 60
                color: ThemeValues.textCol
                font.pixelSize: 14
                font.family: ThemeValues.fontFamily
                clip: true
                selectByMouse: true
                selectedTextColor: ThemeValues.bgCol
                selectionColor: ThemeValues.primaryCol

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Search places..."
                    color: ThemeValues.textCol
                    font.pixelSize: 14
                    font.family: ThemeValues.fontFamily
                    opacity: 0.25
                    visible: !searchInput.text && !searchInput.activeFocus
                }

                onTextChanged: {
                    if (text.length >= 3) {
                        searchDebounce.restart()
                    } else {
                        searchResultsVisible = false
                    }
                }

                onAccepted: {
                    if (text.length > 0) {
                        geocode(text, false)
                        searchResultsVisible = false
                        searchInput.focus = false
                    }
                }
            }

            // Clear button
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "\u2715"
                color: ThemeValues.textCol
                font.pixelSize: 14
                opacity: searchInput.text ? 0.5 : 0
                visible: searchInput.text !== ""

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    onClicked: {
                        searchInput.text = ""
                        searchInput.focus = false
                        searchResultsVisible = false
                        searchOverlay.visible = false
                        clearRoute()
                    }
                }
            }
        }
    }

    // Search debounce timer
    Timer {
        id: searchDebounce
        interval: 400
        onTriggered: {
            if (searchInput.text.length >= 3) {
                geocode(searchInput.text, true)
            }
        }
    }

    // Search results dropdown
    Rectangle {
        anchors.top: searchBar.bottom
        anchors.topMargin: 4
        anchors.left: searchBar.left
        anchors.right: searchBar.right
        height: Math.min(searchResults.length * 44, 220)
        radius: 12
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.92)
        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
        border.width: 1
        visible: searchResultsVisible && searchResults.length > 0
        z: 25
        clip: true

        ListView {
            anchors.fill: parent
            model: searchResults

            delegate: Rectangle {
                width: parent ? parent.width : 0
                height: 44
                color: resultMa.pressed
                    ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                    : "transparent"

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 14
                    spacing: 2

                    Text {
                        text: modelData.name || ""
                        color: ThemeValues.textCol
                        font.pixelSize: 13; font.family: ThemeValues.fontFamily
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    Text {
                        text: modelData.context || ""
                        color: ThemeValues.textCol
                        font.pixelSize: 10; font.family: ThemeValues.fontFamily
                        opacity: 0.35
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    height: 1
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.08)
                }

                MouseArea {
                    id: resultMa
                    anchors.fill: parent
                    onClicked: {
                        console.log("Search result clicked:", modelData.name, modelData.lat, modelData.lon)
                        if (mapLoader.item) {
                            mapLoader.item.map.center = QtPositioning.coordinate(
                                modelData.lat, modelData.lon)
                            mapLoader.item.map.zoomLevel = 15
                            followMode = false

                            root._searchCoord = QtPositioning.coordinate(
                                modelData.lat, modelData.lon)
                            searchOverlay.visible = true
                            markerUpdateTimer.restart()
                        }
                        searchInput.text = modelData.name
                        searchResultsVisible = false
                        searchInput.focus = false

                        // Get directions to this result
                        getDirections(modelData.lat, modelData.lon, modelData.name)
                    }
                }
            }
        }
    }

    // ── Turn-by-turn instruction banner ──
    Rectangle {
        id: turnBanner
        anchors.top: searchBar.bottom
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        height: 76
        radius: 14
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.9)
        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.25)
        border.width: 1
        visible: routeActive && routeSteps.length > 0
        z: 20

        Row {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 14

            // Maneuver icon
            Rectangle {
                width: 52; height: 52
                radius: 10
                anchors.verticalCenter: parent.verticalCenter
                color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: maneuverIcon(root.nextManeuver)
                    color: ThemeValues.primaryCol
                    font.pixelSize: 24
                    font.bold: true
                }
            }

            // Instruction text
            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 66 - 80
                spacing: 4

                Text {
                    width: parent.width
                    text: root.nextStepDistance
                    color: ThemeValues.primaryCol
                    font.pixelSize: 20; font.family: ThemeValues.fontFamily; font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: root.nextInstruction
                    color: ThemeValues.textCol
                    font.pixelSize: 13; font.family: ThemeValues.fontFamily
                    elide: Text.ElideRight
                    opacity: 0.7
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                }
            }

            // Step counter
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: (root.currentStep + 1) + "/" + root.routeSteps.length
                color: ThemeValues.textCol
                font.pixelSize: 11; font.family: ThemeValues.fontFamily
                opacity: 0.35
            }
        }
    }

    // ── Left control strip (below search bar) ──
    Column {
        id: controlColumn
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.bottom: bottomBar.top
        anchors.bottomMargin: 12
        spacing: 8
        z: 15

        MapButton {
            text: "+"
            onClicked: {
                if (mapLoader.item)
                    mapLoader.item.map.zoomLevel = Math.min(mapLoader.item.map.zoomLevel + 1, 20)
            }
        }

        MapButton {
            text: "\u2212"
            onClicked: {
                if (mapLoader.item)
                    mapLoader.item.map.zoomLevel = Math.max(mapLoader.item.map.zoomLevel - 1, 1)
            }
        }

        Item { width: 1; height: 8 }

        MapButton {
            text: "\u2316"
            highlighted: followMode
            onClicked: {
                followMode = !followMode
                if (followMode && gps.position.coordinateValid && mapLoader.item) {
                    mapLoader.item.map.center = gps.position.coordinate
                    mapLoader.item.map.zoomLevel = 15
                }
            }
        }

        MapButton {
            text: "3D"
            fontSize: 12
            onClicked: {
                if (!mapLoader.item) return
                var current = mapLoader.item.map.tilt
                mapLoader.item.map.tilt = current > 0 ? 0 : 45
            }
        }

        Item { width: 1; height: 8 }

        MapButton {
            text: "R"
            fontSize: 14
            highlighted: root.radarVisible
            onClicked: toggleRadar()
        }
    }

    // ── Bottom info bar ──
    Rectangle {
        id: bottomBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: routeActive ? 72 : 40
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.8)
        z: 15

        Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        Column {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14

            // Route info row (visible when navigating)
            Item {
                width: parent.width
                height: routeActive ? 32 : 0
                visible: routeActive
                clip: true

                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 16

                    // Distance
                    Row {
                        spacing: 6
                        Text {
                            text: "\u2192"
                            color: ThemeValues.primaryCol
                            font.pixelSize: 14; font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text: root.routeDistance
                            color: ThemeValues.primaryCol
                            font.pixelSize: 16; font.family: ThemeValues.fontFamily; font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Duration
                    Text {
                        text: root.routeDuration
                        color: ThemeValues.textCol
                        font.pixelSize: 14; font.family: ThemeValues.fontFamily
                        opacity: 0.7
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Destination name
                    Text {
                        text: root.routeDestination ? root.routeDestination.name : ""
                        color: ThemeValues.textCol
                        font.pixelSize: 12; font.family: ThemeValues.fontFamily
                        opacity: 0.4
                        elide: Text.ElideRight
                        width: Math.max(0, parent.parent.width - 300)
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Item { width: 1; height: 1 }

                    // Close route button
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 24; height: 24; radius: 12
                        color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                        border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "\u2715"
                            color: ThemeValues.textCol
                            font.pixelSize: 10
                            opacity: 0.7
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: clearRoute()
                        }
                    }
                }
            }

            // Standard info row
            Row {
                width: parent.width
                height: 40
                spacing: 20

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: gps.position.speedValid
                        ? Math.round(gps.position.speed * 3.6) + " km/h"
                        : "--"
                    color: ThemeValues.primaryCol
                    font.pixelSize: 14; font.family: ThemeValues.fontFamily; font.bold: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        if (!mapLoader.item) return ""
                        var c = mapLoader.item.map.center
                        return c.latitude.toFixed(4) + ", " + c.longitude.toFixed(4)
                    }
                    color: ThemeValues.textCol
                    font.pixelSize: 11; font.family: ThemeValues.fontFamily
                    opacity: 0.4
                }

                Item { width: 1; height: 1 }

                // Style selector pills
                Repeater {
                    model: [
                        { label: "Dark", name: "Dark" },
                        { label: "Street", name: "Streets" },
                        { label: "Sat", name: "Satellite Streets" }
                    ]

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: pillLabel.width + 14; height: 24; radius: 12
                        color: activeStyleName === modelData.name
                            ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                            : Qt.rgba(0.2, 0.2, 0.2, 0.3)
                        border.color: activeStyleName === modelData.name
                            ? ThemeValues.primaryCol
                            : Qt.rgba(0.5, 0.5, 0.5, 0.2)
                        border.width: 1

                        Text {
                            id: pillLabel
                            anchors.centerIn: parent
                            text: modelData.label
                            color: activeStyleName === modelData.name ? ThemeValues.primaryCol : ThemeValues.textCol
                            font.pixelSize: 10; font.family: ThemeValues.fontFamily; font.bold: true
                            opacity: activeStyleName === modelData.name ? 1.0 : 0.5
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: selectStyle(modelData.name)
                        }
                    }
                }
            }
        }
    }

    // ── Geocoding ──
    function geocode(query, suggestions) {
        var limit = suggestions ? 5 : 1
        var url = "https://api.mapbox.com/geocoding/v5/mapbox.places/"
            + encodeURIComponent(query)
            + ".json?access_token=" + root.mapboxToken
            + "&limit=" + limit
            + "&language=en"

        // Prefer GPS position for proximity bias, fall back to map center
        if (gps.position.coordinateValid) {
            url += "&proximity=" + gps.position.coordinate.longitude + "," + gps.position.coordinate.latitude
        } else if (mapLoader.item) {
            var c = mapLoader.item.map.center
            url += "&proximity=" + c.longitude + "," + c.latitude
        }

        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                try {
                    var result = JSON.parse(xhr.responseText)
                    if (!result.features || result.features.length === 0) return

                    if (suggestions) {
                        var results = []
                        for (var i = 0; i < result.features.length; i++) {
                            var f = result.features[i]
                            results.push({
                                name: f.text || f.place_name,
                                context: f.place_name,
                                lat: f.center[1],
                                lon: f.center[0]
                            })
                        }
                        searchResults = results
                        searchResultsVisible = true
                    } else {
                        var coords = result.features[0].center
                        console.log("Geocode result:", result.features[0].place_name, "at", coords[1], coords[0])
                        if (mapLoader.item) {
                            mapLoader.item.map.center = QtPositioning.coordinate(coords[1], coords[0])
                            mapLoader.item.map.zoomLevel = 15
                            followMode = false

                            root._searchCoord = QtPositioning.coordinate(coords[1], coords[0])
                            searchOverlay.visible = true
                            markerUpdateTimer.restart()

                            getDirections(coords[1], coords[0], result.features[0].place_name || "")
                        }
                    }
                } catch (e) {
                    console.error("Geocode error:", e)
                }
            }
        }
        xhr.open("GET", url)
        xhr.send()
    }

    // ── Directions (Mapbox Directions API) ──
    function getDirections(destLat, destLon, destName) {
        // Use GPS if available, otherwise use current map center
        var origin
        if (gps.position.coordinateValid) {
            origin = gps.position.coordinate
        } else if (mapLoader.item) {
            origin = mapLoader.item.map.center
            console.log("No GPS fix — using map center as origin")
        } else {
            console.warn("No GPS fix and no map — cannot calculate route")
            return
        }
        var url = "https://api.mapbox.com/directions/v5/mapbox/driving/"
            + origin.longitude + "," + origin.latitude + ";"
            + destLon + "," + destLat
            + "?geometries=geojson&overview=full&steps=true&banner_instructions=true&annotations=maxspeed&access_token=" + root.mapboxToken

        var xhr = new XMLHttpRequest()
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
                try {
                    var result = JSON.parse(xhr.responseText)
                    if (!result.routes || result.routes.length === 0) {
                        console.warn("No routes found")
                        return
                    }

                    var route = result.routes[0]

                    // Format distance
                    var distM = route.distance
                    if (distM >= 1000) {
                        routeDistance = (distM / 1000).toFixed(1) + " km"
                    } else {
                        routeDistance = Math.round(distM) + " m"
                    }

                    // Format duration
                    var durSec = route.duration
                    if (durSec >= 3600) {
                        var hrs = Math.floor(durSec / 3600)
                        var mins = Math.round((durSec % 3600) / 60)
                        routeDuration = hrs + " hr " + mins + " min"
                    } else {
                        routeDuration = Math.round(durSec / 60) + " min"
                    }

                    routeDestination = { lat: destLat, lon: destLon, name: destName }
                    root._destCoord = QtPositioning.coordinate(destLat, destLon)
                    markerUpdateTimer.restart()

                    // Build GeoJSON for the route line
                    routeGeoJson = {
                        "type": "Feature",
                        "geometry": route.geometry
                    }

                    routeActive = true

                    // Parse turn-by-turn steps
                    var legs = route.legs
                    var steps = []
                    if (legs && legs.length > 0) {
                        for (var s = 0; s < legs[0].steps.length; s++) {
                            var step = legs[0].steps[s]
                            var maneuver = step.maneuver || {}
                            steps.push({
                                instruction: maneuver.instruction || "",
                                type: maneuver.type || "",
                                modifier: maneuver.modifier || "",
                                distance: step.distance || 0,
                                duration: step.duration || 0,
                                name: step.name || "",
                                lat: maneuver.location ? maneuver.location[1] : 0,
                                lon: maneuver.location ? maneuver.location[0] : 0
                            })
                        }
                    }
                    routeSteps = steps
                    currentStep = 0
                    updateCurrentStepDisplay()

                    // Fit map to show entire route
                    fitRouteBounds(origin.latitude, origin.longitude, destLat, destLon)

                    // Feed route coordinates to RouteWeatherManager for weather-along-route tracking
                    if (typeof routeWeatherManager !== 'undefined' && route.geometry && route.geometry.coordinates) {
                        var coords = route.geometry.coordinates
                        routeWeatherManager.setRouteCoordinates(coords, durSec)
                        console.log("Maps: Sent", coords.length, "route coords to RouteWeatherManager, duration:", durSec, "sec")
                    }

                    // Feed route coordinates to RoadConditionManager for 511/DriveBC alerts
                    if (typeof roadConditionManager !== 'undefined' && route.geometry && route.geometry.coordinates) {
                        roadConditionManager.setRouteCoordinates(route.geometry.coordinates, durSec)
                        console.log("Maps: Sent route coords to RoadConditionManager")
                    }

                    // Feed speed limit annotations to SpeedLimitManager
                    if (typeof speedLimitManager !== 'undefined' && route.legs && route.legs.length > 0) {
                        var annotation = route.legs[0].annotation
                        if (annotation && annotation.maxspeed) {
                            speedLimitManager.setSpeedLimitData(annotation.maxspeed, route.geometry.coordinates, durSec)
                            console.log("Maps: Sent", annotation.maxspeed.length, "speed limit segments to SpeedLimitManager")
                        }
                    }

                    // Feed route to RoadSurfaceManager for winter road conditions
                    if (typeof roadSurfaceManager !== 'undefined' && route.geometry && route.geometry.coordinates) {
                        roadSurfaceManager.setRouteCoordinates(route.geometry.coordinates, durSec)
                    }

                    // Feed route to HighwayCameraManager
                    if (typeof highwayCameraManager !== 'undefined' && route.geometry && route.geometry.coordinates) {
                        highwayCameraManager.setRouteCoordinates(route.geometry.coordinates, durSec)
                    }

                    // Feed route to AvalancheManager for mountain pass forecasts
                    if (typeof avalancheManager !== 'undefined' && route.geometry && route.geometry.coordinates) {
                        avalancheManager.setRouteCoordinates(route.geometry.coordinates, durSec)
                    }

                    // Feed route to BorderWaitManager (only activates if route is near a crossing)
                    if (typeof borderWaitManager !== 'undefined' && route.geometry && route.geometry.coordinates) {
                        borderWaitManager.setRouteCoordinates(route.geometry.coordinates, durSec)
                    }

                } catch (e) {
                    console.error("Directions error:", e)
                }
            }
        }
        xhr.open("GET", url)
        xhr.send()
    }

    function clearRoute() {
        routeActive = false
        routeDistance = ""
        routeDuration = ""
        routeDestination = null
        root._destCoord = null
        root._searchCoord = null
        routeGeoJson = { "type": "Feature", "geometry": { "type": "LineString", "coordinates": [] } }
        routeSteps = []
        currentStep = 0
        nextManeuver = ""
        nextInstruction = ""
        nextStepDistance = ""
        searchOverlay.visible = false

        // Clear route weather tracking
        if (typeof routeWeatherManager !== 'undefined') {
            routeWeatherManager.clearRoute()
        }

        // Clear road condition tracking
        if (typeof roadConditionManager !== 'undefined') {
            roadConditionManager.clearRoute()
        }

        // Clear all other route-aware managers
        if (typeof speedLimitManager !== 'undefined') speedLimitManager.clearRoute()
        if (typeof roadSurfaceManager !== 'undefined') roadSurfaceManager.clearRoute()
        if (typeof highwayCameraManager !== 'undefined') highwayCameraManager.clearRoute()
        if (typeof avalancheManager !== 'undefined') avalancheManager.clearRoute()
        if (typeof borderWaitManager !== 'undefined') borderWaitManager.clearRoute()
    }

    function fitRouteBounds(lat1, lon1, lat2, lon2) {
        if (!mapLoader.item) return

        var minLat = Math.min(lat1, lat2)
        var maxLat = Math.max(lat1, lat2)
        var minLon = Math.min(lon1, lon2)
        var maxLon = Math.max(lon1, lon2)

        // Add padding
        var latPad = (maxLat - minLat) * 0.2
        var lonPad = (maxLon - minLon) * 0.2

        var centerLat = (minLat + maxLat) / 2
        var centerLon = (minLon + maxLon) / 2

        mapLoader.item.map.center = QtPositioning.coordinate(centerLat, centerLon)

        // Estimate zoom level from bounds span
        var latSpan = maxLat - minLat + latPad * 2
        var lonSpan = maxLon - minLon + lonPad * 2
        var maxSpan = Math.max(latSpan, lonSpan)

        var zoom = 14
        if (maxSpan > 10) zoom = 5
        else if (maxSpan > 5) zoom = 6
        else if (maxSpan > 2) zoom = 7
        else if (maxSpan > 1) zoom = 8
        else if (maxSpan > 0.5) zoom = 9
        else if (maxSpan > 0.2) zoom = 10
        else if (maxSpan > 0.1) zoom = 11
        else if (maxSpan > 0.05) zoom = 12
        else if (maxSpan > 0.02) zoom = 13

        mapLoader.item.map.zoomLevel = zoom
        followMode = false
    }

    // ── Turn-by-turn helpers ──

    function updateCurrentStepDisplay() {
        if (routeSteps.length === 0 || currentStep >= routeSteps.length) {
            nextManeuver = ""
            nextInstruction = ""
            nextStepDistance = ""
            return
        }

        var step = routeSteps[currentStep]
        nextManeuver = step.type + (step.modifier ? "-" + step.modifier : "")
        nextInstruction = step.instruction
        nextStepDistance = formatStepDistance(step.distance)
    }

    function formatStepDistance(meters) {
        if (meters >= 1000)
            return (meters / 1000).toFixed(1) + " km"
        return Math.round(meters) + " m"
    }

    function maneuverIcon(maneuver) {
        // Map Mapbox maneuver types to arrow characters
        if (maneuver.indexOf("arrive") >= 0)        return "\u2691"  // flag
        if (maneuver.indexOf("depart") >= 0)         return "\u2690"  // flag
        if (maneuver.indexOf("straight") >= 0)       return "\u2191"  // up arrow
        if (maneuver.indexOf("slight-right") >= 0)   return "\u2197"  // NE arrow
        if (maneuver.indexOf("right") >= 0)          return "\u2192"  // right arrow
        if (maneuver.indexOf("sharp-right") >= 0)    return "\u21B1"  // hook right
        if (maneuver.indexOf("slight-left") >= 0)    return "\u2196"  // NW arrow
        if (maneuver.indexOf("left") >= 0)           return "\u2190"  // left arrow
        if (maneuver.indexOf("sharp-left") >= 0)     return "\u21B0"  // hook left
        if (maneuver.indexOf("uturn") >= 0)          return "\u21BA"  // CCW arrow
        if (maneuver.indexOf("merge") >= 0)          return "\u2934"  // merge
        if (maneuver.indexOf("roundabout") >= 0)     return "\u21BB"  // CW circle
        if (maneuver.indexOf("rotary") >= 0)         return "\u21BB"
        if (maneuver.indexOf("fork") >= 0)           return "\u2195"  // fork
        if (maneuver.indexOf("ramp") >= 0)           return "\u2197"
        return "\u2191"  // default: straight
    }

    function distanceBetween(lat1, lon1, lat2, lon2) {
        // Haversine formula (returns meters)
        var R = 6371000
        var dLat = (lat2 - lat1) * Math.PI / 180
        var dLon = (lon2 - lon1) * Math.PI / 180
        var a = Math.sin(dLat/2) * Math.sin(dLat/2)
            + Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180)
            * Math.sin(dLon/2) * Math.sin(dLon/2)
        return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a))
    }

    function advanceStepIfNeeded() {
        if (!routeActive || routeSteps.length === 0 || !gps.position.coordinateValid)
            return

        var pos = gps.position.coordinate

        // Check if we're close to the next step's maneuver point
        // Advance if within 30m of the next step
        var nextIdx = currentStep + 1
        if (nextIdx < routeSteps.length) {
            var nextStep = routeSteps[nextIdx]
            var dist = distanceBetween(pos.latitude, pos.longitude, nextStep.lat, nextStep.lon)
            if (dist < 30) {
                currentStep = nextIdx
                updateCurrentStepDisplay()
            }
        }

        // Update distance to next maneuver based on GPS position
        if (currentStep < routeSteps.length) {
            var curStep = routeSteps[currentStep]
            // If there's a next step, show distance to it
            if (currentStep + 1 < routeSteps.length) {
                var target = routeSteps[currentStep + 1]
                var d = distanceBetween(pos.latitude, pos.longitude, target.lat, target.lon)
                nextStepDistance = formatStepDistance(d)
            }
        }

        // Check if we've arrived (close to destination)
        if (routeDestination) {
            var destDist = distanceBetween(pos.latitude, pos.longitude,
                routeDestination.lat, routeDestination.lon)
            if (destDist < 30) {
                nextManeuver = "arrive"
                nextInstruction = "You have arrived"
                nextStepDistance = ""
            }
        }
    }

    // ── Loading overlay ──
    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol
        visible: !mapReady
        z: 100

        Column {
            anchors.centerIn: parent
            spacing: 16

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Loading Map..."
                color: ThemeValues.textCol
                font.pixelSize: 20
                font.family: ThemeValues.fontFamily
                opacity: 0.7
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 200; height: 4
                color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
                radius: 2

                Rectangle {
                    width: parent.width * 0.3
                    height: parent.height
                    color: ThemeValues.primaryCol; radius: 2

                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation { from: 0; to: 140; duration: 1200; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 140; to: 0; duration: 1200; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }
    }

    // ── Inline MapButton component ──
    component MapButton: Rectangle {
        property string text: ""
        property bool highlighted: false
        property int fontSize: 18
        signal clicked()

        width: 48; height: 48; radius: 12
        color: btnMa.pressed
            ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.25)
            : Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.8)
        border.color: highlighted
            ? ThemeValues.primaryCol
            : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)
        border.width: 1

        Behavior on color { ColorAnimation { duration: 100 } }

        Text {
            anchors.centerIn: parent
            text: parent.text
            color: parent.highlighted ? ThemeValues.primaryCol : ThemeValues.textCol
            font.pixelSize: parent.fontSize; font.family: ThemeValues.fontFamily; font.bold: true
            opacity: parent.highlighted ? 1.0 : 0.7
        }

        MouseArea {
            id: btnMa
            anchors.fill: parent
            onClicked: parent.clicked()
        }
    }
}
