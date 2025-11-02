import QtQuick 2.15
import Qt5Compat.GraphicalEffects

Rectangle {
  id: root
  // -------- API --------
  required property var theme
  signal selected(string key)
  signal homeLongPressed()
  property string activeKey: "home"
  property var recentApps: []

  readonly property var allApps: ({
    "home":     { key: "home",     icon: "home" },
    "music":    { key: "music",    icon: "music" },
    "maps":     { key: "maps",     icon: "maps" },
    "settings": { key: "settings", icon: "settings" },
    "phone":    { key: "phone",    icon: "phone" },
    "messages": { key: "messages", icon: "messages" },
    "contacts": { key: "contacts", icon: "contacts" },
    "tidal":    { key: "tidal",    icon: "tidal" },
    "spotify":  { key: "spotify",  icon: "spotify" },
    "radio":    { key: "radio",    icon: "radio" },
    "podcasts": { key: "podcasts", icon: "podcasts" },
    "climate":  { key: "climate",  icon: "climate" },
    "vehicle":  { key: "vehicle",  icon: "vehicle" },
    "camera":   { key: "camera",   icon: "camera" },
    "weather":  { key: "weather",  icon: "weather" }
  })

  // -------- Theme tokens with safe fallbacks --------
  readonly property int _rev: theme ? theme.rev : 0
  readonly property int _pad: {
    if (theme && theme.navbar && theme.navbar.pad !== undefined) {
      return Number(theme.navbar.pad)
    }
    return 8
  }
  readonly property int _iconSz: {
    if (theme && theme.navbar && theme.navbar.iconSize !== undefined) {
      return Number(theme.navbar.iconSize)
    }
    return 28
  }
  readonly property real _pressS: {
    if (theme && theme.navbar && theme.navbar.pressScale !== undefined) {
      return Number(theme.navbar.pressScale)
    }
    return 0.92
  }
  readonly property int _dur: {
    if (theme && theme.motion && theme.motion.dur !== undefined) {
      return Number(theme.motion.dur)
    }
    return 150
  }
  readonly property color _iconCol: {
    if (theme && theme.navbar && theme.navbar.text) {
      return theme.navbar.text
    }
    return "#FFFFFF"
  }
  readonly property color _accent: {
    if (theme && theme.palette && theme.palette.primary) {
      return theme.palette.primary
    }
    return "#00f0ff"
  }
  readonly property color _bg: {
    if (theme && theme.navbar && theme.navbar.bg) {
      return theme.navbar.bg
    }
    return "#000000"
  }

  width: {
    if (theme && theme.navbar && theme.navbar.width !== undefined) {
      return Number(theme.navbar.width)
    }
    return 70
  }
  height: parent ? parent.height : 430
  color: _bg

  readonly property int slotSize: _iconSz + _pad * 2

  // -------- Recent Apps (top 3 slots) --------
  Column {
    id: recentAppsContainer
    anchors.top: parent.top
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.topMargin: _pad
    spacing: _pad

    Repeater {
      model: 3

      delegate: Item {
        id: recentSlot
        width: root.width
        height: root.slotSize

        readonly property int appIndex: index
        readonly property bool hasApp: appIndex < root.recentApps.length
        readonly property string appKey: hasApp ? root.recentApps[appIndex] : ""
        readonly property var appData: hasApp && root.allApps[appKey] ? root.allApps[appKey] : null
        readonly property bool isActive: appKey === root.activeKey

        visible: hasApp

        Rectangle {
          anchors.centerIn: parent
          width: root._iconSz + 12
          height: root._iconSz + 12
          color: "transparent"
          radius: 8
          border.color: parent.isActive ? root._accent : "transparent"
          border.width: 2

          property bool isPressed: false

          // Glow effect when pressed
          layer.enabled: isPressed
          layer.effect: Glow {
            color: root._accent
            spread: 0.5
            radius: 8
            samples: 17
          }

          // Brightness feedback
          opacity: isPressed ? 0.7 : 1.0
          Behavior on opacity {
            NumberAnimation { duration: root._dur }
          }

          Image {
            id: appIcon
            anchors.centerIn: parent
            source: {
              if (parent.parent.appData && root.theme && root.theme.iconPath) {
                return root.theme.iconPath(parent.parent.appData.icon)
              }
              return ""
            }
            width: root._iconSz
            height: root._iconSz
            fillMode: Image.PreserveAspectFit
            smooth: true
            visible: source !== ""

            onStatusChanged: {
              if (status === Image.Error) {
                console.log("Nav icon failed to load:", source)
              } else if (status === Image.Ready) {
                console.log("Nav icon loaded:", source)
              }
            }
          }

          MouseArea {
            anchors.fill: parent
            enabled: parent.parent.hasApp
            onPressed: parent.isPressed = true
            onReleased: parent.isPressed = false
            onClicked: {
              if (parent.parent.appKey) {
                root.activeKey = parent.parent.appKey
                root.selected(parent.parent.appKey)
              }
            }
          }
        }
      }
    }
  }

  // -------- Home Button (bottom, always visible) --------
  Item {
    id: homeSlot
    anchors.bottom: parent.bottom
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottomMargin: _pad
    width: root.width
    height: root.slotSize

    readonly property bool isActive: root.activeKey === "home"

    Rectangle {
      id: homeButtonRect
      anchors.centerIn: parent
      width: root._iconSz + 12
      height: root._iconSz + 12
      color: "transparent"
      radius: 8
      border.color: parent.isActive ? root._accent : "transparent"
      border.width: 2

      property bool isLongPressing: false
      property bool isPressed: false

      // Glow effect when pressing or long pressing
      layer.enabled: isPressed || isLongPressing
      layer.effect: Glow {
        color: root._accent
        spread: 0.5
        radius: 8
        samples: 17
      }

      // Pulsing animation during long press
      SequentialAnimation on scale {
        running: homeButtonRect.isLongPressing
        loops: Animation.Infinite
        NumberAnimation { to: 1.1; duration: 400; easing.type: Easing.InOutQuad }
        NumberAnimation { to: 1.0; duration: 400; easing.type: Easing.InOutQuad }
      }

      // Brightness feedback
      opacity: isPressed ? 0.7 : 1.0
      Behavior on opacity {
        NumberAnimation { duration: root._dur }
      }

      Image {
        id: homeIcon
        anchors.centerIn: parent
        source: (root.theme && root.theme.iconPath) ? root.theme.iconPath("home") : ""
        width: root._iconSz
        height: root._iconSz
        fillMode: Image.PreserveAspectFit
        smooth: true

        onStatusChanged: {
          if (status === Image.Error) {
            console.log("Home icon failed to load:", source)
          } else if (status === Image.Ready) {
            console.log("Home icon loaded:", source)
          }
        }
      }

      Timer {
        id: longPressTimer
        interval: 500
        repeat: false
        onTriggered: {
          console.log("Home button long pressed - activating voice assistant")
          homeButtonRect.isLongPressing = false
          root.homeLongPressed()
        }
      }

      MouseArea {
        anchors.fill: parent

        onPressed: {
          homeButtonRect.isPressed = true
          homeButtonRect.isLongPressing = true
          longPressTimer.start()
        }

        onReleased: {
          homeButtonRect.isPressed = false
          homeButtonRect.isLongPressing = false

          if (longPressTimer.running) {
            longPressTimer.stop()
            root.activeKey = "home"
            root.selected("home")
          }
        }

        onCanceled: {
          homeButtonRect.isPressed = false
          homeButtonRect.isLongPressing = false
          longPressTimer.stop()
        }
      }
    }
  }
}
