import QtQuick 2.15
import Qt5Compat.GraphicalEffects
import HeadUnit

Rectangle {
    id: root

    // API
    property string text: ""
    property string iconKey: ""
    property url    iconSource: ""
    property bool   filled: false
    property int    variant: 0
    signal clicked()

    // Theme hook
    property var theme

    // Internal
    readonly property color _bg: {
        if (filled) return theme?.button?.bg ?? theme?.palette?.primary ?? ThemeValues.primaryCol
        return "transparent"
    }
    readonly property color _fg: {
        if (filled) return theme?.palette?.bg ?? ThemeValues.bgCol
        return theme?.button?.fg ?? theme?.palette?.text ?? ThemeValues.textCol
    }
    readonly property color _border: theme?.button?.border ?? theme?.palette?.primary ?? ThemeValues.primaryCol
    readonly property int   _borderW: filled ? 0 : (theme?.button?.borderWidth ?? 2)
    readonly property int   _rad: theme?.button?.radius ?? theme?.shape?.radius ?? 8

    readonly property int _padH: theme?.button?.padH ?? 16
    readonly property int _padV: theme?.button?.padV ?? 12
    readonly property int _gap: theme?.button?.gap ?? 8

    readonly property int _iconSize: theme?.button?.iconSize ?? 24
    readonly property int _fontSize: theme?.typography?.fontSize ?? 16
    readonly property string _fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"

    readonly property int _animDur: theme?.button?.dur ?? theme?.motion?.duration ?? 150

    readonly property int _themeW: theme?.button?.width  ?? 0
    readonly property int _themeH: theme?.button?.height ?? 0

    // Glow state
    property bool isPressed: false

    color: _bg
    radius: _rad
    border.color: _border
    border.width: _borderW
    opacity: enabled ? 1 : 0.5

    // Implicit size (content), final size (theme if provided)
    implicitWidth:  Math.round(row.implicitWidth  + _padH * 2)
    implicitHeight: Math.round(row.implicitHeight + _padV * 2)
    width:  _themeW > 0 ? _themeW : implicitWidth
    height: _themeH > 0 ? _themeH : implicitHeight

    // Glow effect when pressed
    layer.enabled: isPressed
    layer.effect: Glow {
        color: theme?.palette?.primary ?? _border
        spread: 0.5
        radius: 8
        samples: 17
    }

    // Subtle brightness pulse when pressed
    Behavior on opacity {
        NumberAnimation { duration: _animDur }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: _gap

        // Build theme-driven icon URL
        readonly property string themedRel: {
            if (iconKey && theme && theme.icons && theme.icons[iconKey]) {
                return String(theme.icons[iconKey])
            }
            return ""
        }
        readonly property url themedUrl: {
            if (themedRel !== "" && theme && theme.name) {
                return "qrc:/qt/qml/HeadUnit/themes/" + theme.name + "/" + themedRel
            }
            return ""
        }
        readonly property url resolvedSource: iconSource !== "" ? iconSource : themedUrl

        Image {
            id: icon
            visible: row.resolvedSource !== ""
            source: row.resolvedSource
            width: _iconSize
            height: _iconSize
            fillMode: Image.PreserveAspectFit
            smooth: true
            onStatusChanged: if (status === Image.Error) visible = false
        }

        Text {
            id: label
            text: root.text
            color: _fg
            font.pixelSize: _fontSize
            font.family: _fontFamily
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        onPressed: {
            root.isPressed = true
            root.opacity = 0.85
        }
        onReleased: {
            root.isPressed = false
            root.opacity = root.enabled ? 1 : 0.5
        }
        onClicked: root.clicked()
    }
}
