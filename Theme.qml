import QtQuick 2.15
import QtQml 2.15
import QtCore
import Qt.labs.settings 1.0 as Labs
import HeadUnit

QtObject {
    id: root
    property Labs.Settings prefs: Labs.Settings {
        property string theme: "Cyberpunk"
    }

    property string name: "Cyberpunk"
    signal themeChanged()

    property var _t: ({})
    property int rev: 0

    readonly property var palette:    _t.palette    || ({})
    readonly property var typography: _t.typography || ({})
    readonly property var layout:     _t.layout     || ({})
    readonly property var shape:      _t.shape      || ({})
    readonly property var motion:     _t.motion     || ({})
    readonly property var navbar:     _t.navbar     || ({})
    readonly property var icons:      _t.icons      || ({})
    readonly property var splash:     _t.splash     || ({})

    property var _fontLoader: null

    function iconPath(key) {
        var rel = icons && icons[key]
        return rel ? Qt.resolvedUrl("themes/" + name + "/" + String(rel)) : ""
    }

    function load(themeId) {
        var url = Qt.resolvedUrl("themes/" + themeId + "/tokens.json")
        console.log("Theme load ->", url)

        // Use XMLHttpRequest with synchronous mode for qrc URLs
        var req = new XMLHttpRequest()
        req.open("GET", url, false)  // synchronous for qrc://
        try {
            req.send()
            console.log("Theme status =", req.status, "readyState =", req.readyState)

            var s = req.responseText || ""
            console.log("Theme response length:", s.length)

            if (!s || s.length === 0) {
                console.warn("Theme: Empty response, trying alternative load")
                // Fallback: try loading hardcoded default
                loadDefaultTheme(themeId)
                return
            }

            // Remove BOM if present
            if (s.length && s.charCodeAt(0) === 0xFEFF) {
                s = s.slice(1)
            }

            var parsed = JSON.parse(s)

            // Validate parsed data
            if (!parsed || typeof parsed !== 'object') {
                console.warn("Theme: Invalid JSON structure")
                loadDefaultTheme(themeId)
                return
            }

            root.name = themeId
            root.prefs.theme = themeId
            root._t = parsed

            // Load custom font if specified
            if (parsed.typography && parsed.typography.fontFile) {
                loadCustomFont(themeId, parsed.typography.fontFile)
            }

            root.rev++
            ThemeValues.update(root)
            console.log("Theme loaded successfully:", themeId)
            root.themeChanged()
        } catch (e) {
            console.warn("Theme load error:", e)
            loadDefaultTheme(themeId)
        }
    }

    function loadDefaultTheme(themeId) {
        console.log("Loading hardcoded default theme for:", themeId)
        // Hardcoded Cyberpunk theme as fallback
        var defaultTheme = {
            "palette": {
                "bg": "#0a0a0f",
                "text": "#39ff14",
                "primary": "#00f0ff",
                "accent": "#ff00ff"
            },
            "typography": {
                "fontFamily": "Orbitron",
                "fontFile": "fonts/Orbitron-Regular.ttf",
                "fontSize": 24
            },
            "layout": {
                "pageMargin": 18,
                "gap": 15
            },
            "button": {
                "width": 360,
                "height": 66,
                "radius": 15,
                "bg": "#00141a",
                "fg": "#39ff14",
                "border": "#00f0ff",
                "borderWidth": 2
            },
            "navbar": {
                "width": 138,
                "pad": 18,
                "iconSize": 84,
                "text": "#00f0ff",
                "bg": "#0a0a0f"
            },
            "icons": {
                "home": "icons/home_cyber.png",
                "music": "icons/music_cyber.png",
                "maps": "icons/maps_cyber.png",
                "settings": "icons/settings_cyber.png",
                "phone": "icons/phone_cyber.png"
            },
            "splash": {
                "logo": "logo.svg",
                "bg": "#000000",
                "color": "#00f0ff",
                "glow": "#00f0ff66",
                "primaryText": "CHEVROLET",
                "secondaryText": "AMERICA'S HEARTBEAT",
                "logoSize": 330,
                "titleSize": 60,
                "subtitleSize": 27,
                "holdDuration": 2500,
                "fadeOutDuration": 1200
            }
        }

        root.name = themeId
        root.prefs.theme = themeId
        root._t = defaultTheme

        if (defaultTheme.typography && defaultTheme.typography.fontFile) {
            loadCustomFont(themeId, defaultTheme.typography.fontFile)
        }

        root.rev++
        ThemeValues.update(root)
        console.log("Default theme applied:", themeId)
        root.themeChanged()
    }

    function loadCustomFont(themeId, fontFile) {
        // Clean up previous font loader
        if (_fontLoader) {
            _fontLoader.destroy()
            _fontLoader = null
        }

        if (!fontFile) {
            console.log("No font file specified")
            return
        }

        var fontUrl = Qt.resolvedUrl("themes/" + themeId + "/" + fontFile)
        console.log("Loading font:", fontUrl)

        var comp = Qt.createComponent("FontLoader.qml")
        if (comp.status === Component.Ready) {
            _fontLoader = comp.createObject(root, { source: fontUrl })
            if (!_fontLoader) {
                console.warn("Failed to create font loader object")
            }
        } else if (comp.status === Component.Error) {
            console.warn("Font loader component error:", comp.errorString())
        }
    }

    Component.onCompleted: {
        console.log("Theme: prefs.theme =", prefs.theme, "name =", name)
        console.log("Theme: Starting to load", prefs.theme || name)
        load(prefs.theme || name)
    }
}
