import QtQuick 2.15

Item {
    id: root
    required property var theme
    clip: true

    // Theme tokens with safe fallbacks
    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color textCol: theme?.palette?.text ?? "white"
    readonly property int marginPx: theme?.layout?.pageMargin !== undefined
        ? Number(theme.layout.pageMargin) : 0

    readonly property string ovType: theme?.overlay?.type ? String(theme.overlay.type) : "none"
    readonly property real ovOpacity: theme?.overlay?.opacity !== undefined
        ? Number(theme.overlay.opacity) : 0.14
    readonly property color ovColor: theme?.overlay?.color ?? (textCol === "white" ? "#ffffff" : textCol)
    readonly property real ovSpacing: theme?.overlay?.spacing !== undefined
        ? Number(theme.overlay.spacing) : 12
    readonly property url ovImage: theme?.overlay?.image && theme?.name
        ? "qrc:/qt/qml/HeadUnit/themes/" + theme.name + "/" + String(theme.overlay.image) : ""

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
