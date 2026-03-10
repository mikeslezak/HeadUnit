import QtQuick 2.15
import HeadUnit

Item {
    id: root
    property var theme: null


    // Table selection: 0=VE, 1=Ignition, 2=Lambda
    property int activeTable: 0
    readonly property var tableNames: ["VE %", "Ignition °", "Lambda"]

    // Grid dimensions (matches ECM: 20 RPM × 16 MAP)
    readonly property int rpmBins: 20
    readonly property int mapBins: 16

    // Typical RPM axis labels (500-7000 RPM)
    readonly property var rpmLabels: [
        "500", "750", "1000", "1250", "1500", "1750", "2000", "2500",
        "3000", "3500", "4000", "4250", "4500", "4750", "5000", "5500",
        "6000", "6250", "6500", "7000"
    ]

    // Typical MAP axis labels (20-100 kPa)
    readonly property var mapLabels: [
        "20", "25", "30", "35", "40", "45", "50", "55",
        "60", "65", "70", "75", "80", "85", "90", "100"
    ]

    // Delta tracking: stores pending deltas as "tableId,rpmIdx,mapIdx" -> delta
    property var pendingDeltas: ({})
    property int pendingCount: 0

    // Currently selected cell
    property int selectedRpm: -1
    property int selectedMap: -1

    // Live cell highlight from current RPM/MAP
    readonly property int liveRpmIdx: findClosestIndex(vehicleBusManager.rpm, [
        500, 750, 1000, 1250, 1500, 1750, 2000, 2500,
        3000, 3500, 4000, 4250, 4500, 4750, 5000, 5500,
        6000, 6250, 6500, 7000
    ])
    readonly property int liveMapIdx: findClosestIndex(vehicleBusManager.mapKpa, [
        20, 25, 30, 35, 40, 45, 50, 55,
        60, 65, 70, 75, 80, 85, 90, 100
    ])

    function findClosestIndex(value, axis) {
        var best = 0
        var bestDist = Math.abs(value - axis[0])
        for (var i = 1; i < axis.length; i++) {
            var dist = Math.abs(value - axis[i])
            if (dist < bestDist) {
                bestDist = dist
                best = i
            }
        }
        return best
    }

    function cellKey(rpmIdx, mapIdx) {
        return activeTable + "," + rpmIdx + "," + mapIdx
    }

    function getCellDelta(rpmIdx, mapIdx) {
        var key = cellKey(rpmIdx, mapIdx)
        return pendingDeltas[key] || 0
    }

    function adjustCell(rpmIdx, mapIdx, amount) {
        var key = cellKey(rpmIdx, mapIdx)
        var current = pendingDeltas[key] || 0
        var newVal = current + amount

        var updated = {}
        for (var k in pendingDeltas) updated[k] = pendingDeltas[k]

        if (newVal === 0) {
            delete updated[key]
        } else {
            updated[key] = newVal
        }

        pendingDeltas = updated
        pendingCount = Object.keys(pendingDeltas).length
    }

    function applyDeltas() {
        for (var key in pendingDeltas) {
            var parts = key.split(",")
            var tableId = parseInt(parts[0])
            var rpmIdx = parseInt(parts[1])
            var mapIdx = parseInt(parts[2])
            var delta = pendingDeltas[key]
            // Delta is ×10 on wire (VE: 0.1% resolution, Ign: 0.1° resolution)
            vehicleBusManager.sendTuneDelta(tableId, rpmIdx, mapIdx, delta * 10)
        }
        pendingDeltas = ({})
        pendingCount = 0
    }

    function rollbackAll() {
        vehicleBusManager.sendTuneRollback()
        pendingDeltas = ({})
        pendingCount = 0
    }

    // Handle tune ACKs from ECM
    Connections {
        target: vehicleBusManager
        function onTuneAckReceived(tableId, rpmIdx, mapIdx, accepted) {
            if (!accepted) {
                console.warn("Tuning: ECM rejected delta at", rpmIdx, mapIdx)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        // Not connected state
        Column {
            anchors.centerIn: parent
            spacing: 16
            visible: !vehicleBusManager.ecmOnline

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "ECM Offline"
                color: ThemeValues.textCol; font.pixelSize: 22; font.family: ThemeValues.fontFamily
                opacity: 0.6
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "ECM must be online to access tuning tables"
                color: ThemeValues.textCol; font.pixelSize: 14; font.family: ThemeValues.fontFamily
                opacity: 0.3
            }
        }

        // Main tuning UI
        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8
            visible: vehicleBusManager.ecmOnline || true  // Show always for layout dev

            // Header row
            Item {
                width: parent.width
                height: 32

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Text {
                        text: "TUNE"
                        color: ThemeValues.textCol; font.pixelSize: 20; font.family: ThemeValues.fontFamily; font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Table selector tabs
                    Repeater {
                        model: tableNames
                        Rectangle {
                            width: tabText.width + 16; height: 26; radius: 6
                            color: activeTable === index
                                ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.25)
                                : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.06)
                            border.color: activeTable === index ? ThemeValues.primaryCol : "transparent"
                            border.width: 1
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                id: tabText
                                anchors.centerIn: parent
                                text: modelData
                                color: activeTable === index ? ThemeValues.primaryCol : ThemeValues.textCol
                                font.pixelSize: 11; font.family: ThemeValues.fontFamily; font.bold: true
                                opacity: activeTable === index ? 1.0 : 0.5
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: { activeTable = index; selectedRpm = -1; selectedMap = -1 }
                            }
                        }
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    // Live RPM/MAP readout
                    Text {
                        text: vehicleBusManager.rpm + " RPM / " + vehicleBusManager.mapKpa + " kPa"
                        color: ThemeValues.primaryCol; font.pixelSize: 11; font.family: ThemeValues.fontFamily
                        opacity: 0.7
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Pending count badge
                    Rectangle {
                        width: pendingText.width + 12; height: 22; radius: 4
                        color: pendingCount > 0
                            ? Qt.rgba(ThemeValues.accentCol.r, ThemeValues.accentCol.g, ThemeValues.accentCol.b, 0.2)
                            : Qt.rgba(0.3, 0.3, 0.3, 0.2)
                        visible: pendingCount > 0
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            id: pendingText
                            anchors.centerIn: parent
                            text: pendingCount + " pending"
                            color: ThemeValues.accentCol; font.pixelSize: 10; font.family: ThemeValues.fontFamily
                        }
                    }

                    // Apply button
                    Rectangle {
                        width: 60; height: 26; radius: 6
                        color: pendingCount > 0
                            ? Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.2)
                            : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.1)
                        border.color: pendingCount > 0 ? ThemeValues.successCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: pendingCount > 0 ? 1.0 : 0.4

                        Text {
                            anchors.centerIn: parent
                            text: "APPLY"
                            color: pendingCount > 0 ? ThemeValues.successCol : Qt.rgba(ThemeValues.textCol.r, ThemeValues.textCol.g, ThemeValues.textCol.b, 0.3)
                            font.pixelSize: 10; font.family: ThemeValues.fontFamily; font.bold: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: pendingCount > 0
                            onClicked: applyDeltas()
                        }
                    }

                    // Rollback button
                    Rectangle {
                        width: 70; height: 26; radius: 6
                        color: Qt.rgba(ThemeValues.accentCol.r, ThemeValues.accentCol.g, ThemeValues.accentCol.b, 0.1)
                        border.color: Qt.rgba(ThemeValues.accentCol.r, ThemeValues.accentCol.g, ThemeValues.accentCol.b, 0.3)
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "ROLLBACK"
                            color: ThemeValues.accentCol; font.pixelSize: 10; font.family: ThemeValues.fontFamily; font.bold: true
                            opacity: 0.7
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: rollbackAll()
                        }
                    }
                }
            }

            // 2D Map Grid
            Item {
                width: parent.width
                height: parent.height - 32 - 8 - cellInfo.height - 8

                // MAP axis labels (vertical, left side)
                Column {
                    id: mapAxisLabels
                    anchors.left: parent.left
                    anchors.top: gridContainer.top
                    width: 30

                    Repeater {
                        model: mapBins
                        Text {
                            width: 30
                            height: gridContainer.height / mapBins
                            text: mapLabels[mapBins - 1 - index]
                            color: ThemeValues.textCol; font.pixelSize: 8; font.family: ThemeValues.fontFamily
                            opacity: 0.4
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: Text.AlignVCenter
                            rightPadding: 4
                        }
                    }
                }

                // MAP axis title
                Text {
                    anchors.left: parent.left
                    anchors.bottom: mapAxisLabels.top
                    anchors.bottomMargin: 2
                    text: "MAP kPa"
                    color: ThemeValues.textCol; font.pixelSize: 8; font.family: ThemeValues.fontFamily
                    opacity: 0.3
                }

                // Grid area
                Item {
                    id: gridContainer
                    anchors.left: mapAxisLabels.right
                    anchors.leftMargin: 4
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    anchors.bottom: rpmAxisLabels.top
                    anchors.bottomMargin: 4

                    readonly property real cellW: width / rpmBins
                    readonly property real cellH: height / mapBins

                    // Grid cells
                    Repeater {
                        model: rpmBins * mapBins

                        Rectangle {
                            readonly property int rpmIdx: index % rpmBins
                            readonly property int mapIdx: mapBins - 1 - Math.floor(index / rpmBins)
                            readonly property int delta: getCellDelta(rpmIdx, mapIdx)
                            readonly property bool isLive: rpmIdx === liveRpmIdx && mapIdx === liveMapIdx
                            readonly property bool isSelected: rpmIdx === selectedRpm && mapIdx === selectedMap

                            x: rpmIdx * gridContainer.cellW
                            y: Math.floor(index / rpmBins) * gridContainer.cellH
                            width: gridContainer.cellW - 1
                            height: gridContainer.cellH - 1

                            color: {
                                if (delta > 0) return Qt.rgba(ThemeValues.successCol.r, ThemeValues.successCol.g, ThemeValues.successCol.b, 0.15 + Math.min(Math.abs(delta) * 0.03, 0.5))
                                if (delta < 0) return Qt.rgba(ThemeValues.errorCol.r, ThemeValues.errorCol.g, ThemeValues.errorCol.b, 0.15 + Math.min(Math.abs(delta) * 0.03, 0.5))
                                return Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.04)
                            }
                            radius: 2

                            border.color: {
                                if (isSelected) return ThemeValues.primaryCol
                                if (isLive) return ThemeValues.warningCol
                                return "transparent"
                            }
                            border.width: (isSelected || isLive) ? 2 : 0

                            // Delta value text
                            Text {
                                anchors.centerIn: parent
                                text: delta !== 0 ? (delta > 0 ? "+" : "") + delta : ""
                                color: delta > 0 ? ThemeValues.successCol : ThemeValues.accentCol
                                font.pixelSize: Math.min(gridContainer.cellW, gridContainer.cellH) * 0.4
                                font.family: ThemeValues.fontFamily; font.bold: true
                                visible: delta !== 0
                            }

                            // Live dot indicator
                            Rectangle {
                                width: 4; height: 4; radius: 2
                                color: ThemeValues.warningCol
                                anchors.top: parent.top; anchors.topMargin: 1
                                anchors.right: parent.right; anchors.rightMargin: 1
                                visible: isLive && delta === 0
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    selectedRpm = rpmIdx
                                    selectedMap = mapIdx
                                }
                            }
                        }
                    }
                }

                // RPM axis labels (horizontal, bottom)
                Row {
                    id: rpmAxisLabels
                    anchors.left: gridContainer.left
                    anchors.bottom: parent.bottom
                    width: gridContainer.width

                    Repeater {
                        model: rpmBins
                        Text {
                            width: gridContainer.width / rpmBins
                            text: index % 2 === 0 ? rpmLabels[index] : ""
                            color: ThemeValues.textCol; font.pixelSize: 8; font.family: ThemeValues.fontFamily
                            opacity: 0.4
                            horizontalAlignment: Text.AlignHCenter
                            rotation: -45
                            transformOrigin: Item.Top
                        }
                    }
                }

                // RPM axis title
                Text {
                    anchors.horizontalCenter: gridContainer.horizontalCenter
                    anchors.bottom: parent.bottom
                    text: "RPM"
                    color: ThemeValues.textCol; font.pixelSize: 8; font.family: ThemeValues.fontFamily
                    opacity: 0.3
                }
            }

            // Cell info & adjustment bar
            Item {
                id: cellInfo
                width: parent.width
                height: 44

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.04)
                    radius: 8
                    border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.1)
                    border.width: 1
                }

                Row {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 16

                    // Selected cell info
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: selectedRpm >= 0
                            ? rpmLabels[selectedRpm] + " RPM / " + mapLabels[selectedMap] + " kPa"
                            : "Tap a cell to select"
                        color: ThemeValues.textCol; font.pixelSize: 13; font.family: ThemeValues.fontFamily
                        opacity: selectedRpm >= 0 ? 0.8 : 0.3
                        width: 180
                    }

                    // Current delta display
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            if (selectedRpm < 0) return ""
                            var d = getCellDelta(selectedRpm, selectedMap)
                            var unit = activeTable === 0 ? "%" : activeTable === 1 ? "°" : "λ"
                            return "Delta: " + (d > 0 ? "+" : "") + d + " " + unit
                        }
                        color: {
                            var d = getCellDelta(selectedRpm, selectedMap)
                            return d > 0 ? ThemeValues.successCol : d < 0 ? ThemeValues.accentCol : ThemeValues.textCol
                        }
                        font.pixelSize: 14; font.family: ThemeValues.fontFamily; font.bold: true
                        opacity: selectedRpm >= 0 ? 1.0 : 0
                        width: 120
                    }

                    Item { width: 1; height: 1 }

                    // Adjustment buttons
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        visible: selectedRpm >= 0

                        Repeater {
                            model: [
                                { label: "-5", val: -5 },
                                { label: "-1", val: -1 },
                                { label: "+1", val: 1 },
                                { label: "+5", val: 5 }
                            ]

                            Rectangle {
                                width: 44; height: 30; radius: 6
                                color: adjMa.pressed
                                    ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                                    : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.08)
                                border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                                border.width: 1
                                anchors.verticalCenter: parent.verticalCenter

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: modelData.val > 0 ? ThemeValues.successCol : ThemeValues.accentCol
                                    font.pixelSize: 12; font.family: ThemeValues.fontFamily; font.bold: true
                                }
                                MouseArea {
                                    id: adjMa
                                    anchors.fill: parent
                                    onClicked: adjustCell(selectedRpm, selectedMap, modelData.val)
                                }
                            }
                        }

                        // Zero button
                        Rectangle {
                            width: 36; height: 30; radius: 6
                            color: Qt.rgba(0.5, 0.5, 0.5, 0.1)
                            border.color: Qt.rgba(0.5, 0.5, 0.5, 0.2)
                            border.width: 1
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "0"
                                color: ThemeValues.textCol; font.pixelSize: 12; font.family: ThemeValues.fontFamily
                                opacity: 0.5
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    var key = cellKey(selectedRpm, selectedMap)
                                    var updated = {}
                                    for (var k in pendingDeltas) {
                                        if (k !== key) updated[k] = pendingDeltas[k]
                                    }
                                    pendingDeltas = updated
                                    pendingCount = Object.keys(pendingDeltas).length
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
