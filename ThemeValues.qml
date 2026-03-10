pragma Singleton
import QtQuick 2.15

/**
 * ThemeValues - Singleton providing resolved theme colors, fonts, and layout values.
 *
 * Populated by Theme.qml after loading. All QML files can reference these
 * properties directly (e.g., ThemeValues.primaryCol) without needing
 * a `property var theme` passed in from their parent.
 */
QtObject {
    // Core palette
    property color bgCol: "#0a0a0f"
    property color textCol: "#39ff14"
    property color primaryCol: "#00f0ff"
    property color accentCol: "#ff006e"
    property color cardBgCol: "#1a1a1f"

    // Semantic colors (call UI, status indicators, etc.)
    property color successCol: "#2ECC71"
    property color errorCol: "#E63946"
    property color warningCol: "#F5A623"

    // Typography
    property string fontFamily: "Noto Sans"
    property int fontSize: 16

    // Layout
    property int pageMargin: 18
    property int gap: 15

    // Shape
    property int radius: 8
    property int borderWidth: 2

    // Raw theme object (for components that need full access, e.g., NavBar icons, SplashScreen)
    property var theme: null

    // Revision counter — increments on theme change so bindings update
    property int rev: 0

    function update(t) {
        if (!t) return
        theme = t

        bgCol = t.palette?.bg ?? "#0a0a0f"
        textCol = t.palette?.text ?? "#39ff14"
        primaryCol = t.palette?.primary ?? "#00f0ff"
        accentCol = t.palette?.accent ?? "#ff006e"
        cardBgCol = t.palette?.cardBg ?? "#1a1a1f"

        successCol = t.palette?.success ?? "#2ECC71"
        errorCol = t.palette?.error ?? "#E63946"
        warningCol = t.palette?.warning ?? "#F5A623"

        fontFamily = t.typography?.fontFamily ?? "Noto Sans"
        fontSize = t.typography?.fontSize ? Number(t.typography.fontSize) : 16

        pageMargin = t.layout?.pageMargin !== undefined ? Number(t.layout.pageMargin) : 18
        gap = t.layout?.gap !== undefined ? Number(t.layout.gap) : 15

        radius = t.shape?.radius !== undefined ? Number(t.shape.radius) : 8
        borderWidth = t.shape?.borderWidth !== undefined ? Number(t.shape.borderWidth) : 2

        rev++
    }
}
