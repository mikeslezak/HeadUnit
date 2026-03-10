import QtQuick 2.15
import HeadUnit

Flickable {
    id: root
    anchors.fill: parent
    contentHeight: col.height + 40
    clip: true

    property var theme: null
    property var appSettings: null

    Column {
        id: col
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        spacing: 20
        width: parent.width - 40

        Rectangle {
            width: 120; height: 120; color: "transparent"
            border.color: ThemeValues.primaryCol; border.width: 2; radius: 60
            anchors.horizontalCenter: parent.horizontalCenter
            Text { anchors.centerIn: parent; text: "\uD83D\uDE97"; font.pixelSize: 60 }
        }

        Text {
            text: "HeadUnit"
            color: ThemeValues.textCol
            font.pixelSize: ThemeValues.fontSize + 8; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Column {
            width: parent.width
            spacing: 12
            anchors.horizontalCenter: parent.horizontalCenter

            InfoRow { label: "Version"; value: updateManager ? updateManager.currentVersion : "?" }
            InfoRow { label: "Commit"; value: updateManager ? updateManager.currentCommit : "?" }
            InfoRow { label: "Qt Version"; value: "6.5.3" }
            InfoRow { label: "Platform"; value: Qt.platform.os + " (aarch64)" }
        }

        Rectangle {
            width: parent.width; height: 1
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        }

        // ── Software Update Section ──
        Text {
            text: "Software Update"
            color: ThemeValues.primaryCol
            font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
        }

        // Status message
        Text {
            width: parent.width
            text: updateManager ? updateManager.statusMessage : ""
            color: ThemeValues.textCol
            font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily
            opacity: 0.6
            visible: text !== ""
            wrapMode: Text.WordWrap
        }

        // Latest commit info (when update available)
        Rectangle {
            width: parent.width; height: updateInfoCol.height + 16
            radius: 8
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.08)
            border.color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
            border.width: 1
            visible: updateManager && updateManager.updateAvailable

            Column {
                id: updateInfoCol
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top; anchors.margins: 8
                spacing: 4

                Text {
                    text: "Latest: " + (updateManager ? updateManager.latestCommit : "")
                    color: ThemeValues.primaryCol
                    font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily; font.bold: true
                }
                Text {
                    width: parent.width
                    text: updateManager ? updateManager.latestMessage : ""
                    color: ThemeValues.textCol
                    font.pixelSize: ThemeValues.fontSize - 3; font.family: ThemeValues.fontFamily
                    opacity: 0.6
                    elide: Text.ElideRight
                }
            }
        }

        // Progress bar (during update)
        Rectangle {
            width: parent.width; height: 6; radius: 3
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.1)
            visible: updateManager && updateManager.isUpdating

            Rectangle {
                width: parent.width * (updateManager ? updateManager.updateProgress / 100.0 : 0)
                height: parent.height; radius: 3
                color: ThemeValues.primaryCol

                Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
            }
        }

        // Action buttons
        Row {
            spacing: 12
            anchors.horizontalCenter: parent.horizontalCenter

            // Check for updates button
            Rectangle {
                width: checkText.width + 32; height: 44; radius: 8
                color: checkMa.pressed
                    ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                    : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.1)
                border.color: ThemeValues.primaryCol; border.width: 1
                opacity: (updateManager && (updateManager.isChecking || updateManager.isUpdating)) ? 0.4 : 1.0

                Text {
                    id: checkText
                    anchors.centerIn: parent
                    text: (updateManager && updateManager.isChecking) ? "Checking..." : "Check for Updates"
                    color: ThemeValues.primaryCol
                    font.pixelSize: ThemeValues.fontSize - 1; font.family: ThemeValues.fontFamily; font.bold: true
                }

                MouseArea {
                    id: checkMa
                    anchors.fill: parent
                    onClicked: {
                        if (updateManager && !updateManager.isChecking && !updateManager.isUpdating) {
                            updateManager.checkForUpdates()
                        }
                    }
                }
            }

            // Install update button (visible when update available)
            Rectangle {
                width: installText.width + 32; height: 44; radius: 8
                color: installMa.pressed
                    ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.4)
                    : ThemeValues.primaryCol
                visible: updateManager && updateManager.updateAvailable && !updateManager.isUpdating
                opacity: (updateManager && updateManager.isUpdating) ? 0.4 : 1.0

                Text {
                    id: installText
                    anchors.centerIn: parent
                    text: "Install Update"
                    color: ThemeValues.bgCol
                    font.pixelSize: ThemeValues.fontSize - 1; font.family: ThemeValues.fontFamily; font.bold: true
                }

                MouseArea {
                    id: installMa
                    anchors.fill: parent
                    onClicked: {
                        if (updateManager && !updateManager.isUpdating) {
                            updateManager.startUpdate()
                        }
                    }
                }
            }

            // Restart button (after successful update)
            Rectangle {
                width: restartText.width + 32; height: 44; radius: 8
                color: restartMa.pressed
                    ? Qt.rgba(1, 0.3, 0.3, 0.4)
                    : Qt.rgba(1, 0.3, 0.3, 0.15)
                border.color: Qt.rgba(1, 0.3, 0.3, 0.6); border.width: 1
                visible: updateManager && updateManager.updateProgress === 100 && !updateManager.isUpdating

                Text {
                    id: restartText
                    anchors.centerIn: parent
                    text: "Restart App"
                    color: Qt.rgba(1, 0.4, 0.4, 1)
                    font.pixelSize: ThemeValues.fontSize - 1; font.family: ThemeValues.fontFamily; font.bold: true
                }

                MouseArea {
                    id: restartMa
                    anchors.fill: parent
                    onClicked: Qt.quit()
                }
            }
        }

        Rectangle {
            width: parent.width; height: 1
            color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
        }

        Text {
            text: "\u00A9 2025 Custom Head Unit Project"
            color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily; opacity: 0.5
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    // --- Inline Components ---

    component InfoRow: Row {
        width: parent.width; spacing: 12
        property string label: ""; property string value: ""
        Text { text: label + ":"; color: ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; width: 120 }
        Text { text: value; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily }
    }
}
