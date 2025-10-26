import QtQuick 2.15
import QtQml 2.15
import QtCore
import Qt.labs.settings

QtObject {
    id: root
    property Settings prefs: Settings {
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
        var req = new XMLHttpRequest()
        req.open("GET", url)
        req.onreadystatechange = function () {
            if (req.readyState !== XMLHttpRequest.DONE) return
            console.log("Theme status =", req.status)
            if (req.status !== 0 && req.status !== 200) {
                console.warn("Theme HTTP error:", req.status)
                return
            }
            try {
                var s = req.responseText || ""
                if (!s || s.length === 0) {
                    console.warn("Theme: Empty response")
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
                    return
                }

                root._t = parsed
                root.name = themeId
                root.prefs.theme = themeId

                // Load custom font if specified
                if (parsed.typography && parsed.typography.fontFile) {
                    loadCustomFont(themeId, parsed.typography.fontFile)
                }

                root.rev++
                console.log("Theme loaded successfully:", themeId)
                root.themeChanged()
            } catch (e) {
                console.warn("Theme parse error:", e, "Response:", req.responseText)
            }
        }
        req.send()
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
        console.log("Theme: Starting to load", prefs.theme || name)
        load(prefs.theme || name)
    }
}
