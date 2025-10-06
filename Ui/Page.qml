import QtQuick 2.15

Item {
    id: root
    required property var theme
    clip: true

    // Theme tokens with safe fallbacks
    readonly property color bgCol: {
        if (theme && theme.palette && theme.palette.bg) {
            return theme.palette.bg
        }
        return "#0a0a0f"
    }
    readonly property color textCol: {
        if (theme && theme.palette && theme.palette.text) {
            return theme.palette.text
        }
        return "white"
    }
    readonly property int marginPx: {
        if (theme && theme.layout && theme.layout.pageMargin !== undefined) {
            return Number(theme.layout.pageMargin)
        }
        return 0
    }

    readonly property string ovType: {
        if (theme && theme.overlay && theme.overlay.type) {
            return String(theme.overlay.type)
        }
        return "none"
    }
    readonly property real ovOpacity: {
        if (theme && theme.overlay && theme.overlay.opacity !== undefined) {
            return Number(theme.overlay.opacity)
        }
        return 0.14
    }
    readonly property color ovColor: {
        if (theme && theme.overlay && theme.overlay.color) {
            return theme.overlay.color
        }
        return (textCol === "white" ? "#ffffff" : textCol)
    }
    readonly property real ovSpacing: {
        if (theme && theme.overlay && theme.overlay.spacing !== undefined) {
            return Number(theme.overlay.spacing)
        }
        return 12
    }
    readonly property url ovImage: {
        if (theme && theme.overlay && theme.overlay.image && theme.name) {
            return "qrc:/qt/qml/HeadUnit/themes/" + theme.name + "/" + String(theme.overlay.image)
        }
        return ""
    }

    // Background at the very back
    Rectangle {
        anchors.fill: parent
        color: bgCol
        z: 0
    }

    // --- overlays BEHIND content & non-interactive ---
    Image {
        id: noise
        anchors.fill: parent
        z: 1
        enabled: false                   // don't take events
        visible: ovType === "noise" && ovImage !== ""
        source: ovImage
        fillMode: Image.Tile
        opacity: ovOpacity
        smooth: true
        onStatusChanged: if (status === Image.Error) visible = false
    }

    Canvas {
        id: grid
        anchors.fill: parent
        z: 1
        enabled: false                   // don't take events
        visible: ovType === "grid"
        opacity: ovOpacity
        renderTarget: Canvas.FramebufferObject
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = ovColor
            ctx.lineWidth = 1
            const s = Math.max(4, ovSpacing)
            for (let x = 0.5; x <= width; x += s) { ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,height); ctx.stroke(); }
            for (let y = 0.5; y <= height; y += s) { ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(width,y); ctx.stroke(); }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections {
            target: theme
            function onThemeChanged() {
                if (grid.visible) {
                    grid.requestPaint()
                }
            }
        }
    }

    Repeater {
        id: scanlines
        z: 1
        enabled: false                   // don't take events
        model: (ovType === "scanlines") ? Math.ceil(height / ovSpacing) : 0
        Rectangle {
            width: root.width
            height: Math.max(1, Math.floor(ovSpacing * 0.15))
            y: index * ovSpacing
            color: ovColor
            opacity: ovOpacity
        }
    }

    // --- content goes on TOP of overlays ---
    default property alias content: contentItem.data
    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: marginPx
        z: 10
    }
}
