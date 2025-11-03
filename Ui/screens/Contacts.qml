import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQml.Models 2.15

Item {
    id: root
    property var theme: null

    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color cardBgCol: theme?.palette?.cardBg ?? "#1a1a1f"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"

    // Filtered model for search
    DelegateModel {
        id: filteredModel
        model: contactManager ? contactManager.contactModel : null

        filterOnGroup: "filtered"

        groups: [
            DelegateModelGroup {
                id: filteredGroup
                name: "filtered"
                includeByDefault: true
            }
        ]

        items.onChanged: updateFilter()

        function updateFilter() {
            var searchText = searchField.text.toLowerCase().trim()

            if (searchText === "") {
                // Show all contacts
                for (var i = 0; i < items.count; i++) {
                    var item = items.get(i)
                    item.inFiltered = true
                }
            } else {
                // Filter contacts
                for (var i = 0; i < items.count; i++) {
                    var item = items.get(i)
                    var contactName = item.model.name ? item.model.name.toLowerCase() : ""
                    var contactPhone = item.model.phoneNumber ? item.model.phoneNumber : ""
                    var contactEmail = item.model.email ? item.model.email.toLowerCase() : ""

                    var matches = contactName.indexOf(searchText) !== -1 ||
                                  contactPhone.indexOf(searchText) !== -1 ||
                                  contactEmail.indexOf(searchText) !== -1

                    item.inFiltered = matches
                }
            }
        }

        delegate: Rectangle {
            width: contactList.width
            height: 70
            color: "transparent"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 5
                color: contactMouseArea.pressed ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2) :
                       contactMouseArea.containsMouse ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.1) :
                       Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.5)
                border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                border.width: 1
                radius: 4

                Behavior on color { ColorAnimation { duration: 150 } }

                Row {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 15

                    // Avatar (first letter)
                    Rectangle {
                        width: 50
                        height: 50
                        anchors.verticalCenter: parent.verticalCenter
                        color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                        border.color: primaryCol
                        border.width: 1
                        radius: 25

                        Text {
                            anchors.centerIn: parent
                            text: model.firstLetter
                            color: primaryCol
                            font.pixelSize: 24
                            font.family: fontFamily
                            font.weight: Font.Bold
                        }
                    }

                    // Contact info
                    Column {
                        width: parent.width - 140
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Text {
                            text: model.name || "Unknown"
                            color: textCol
                            font.pixelSize: 16
                            font.family: fontFamily
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                            width: parent.width
                        }

                        Text {
                            text: model.phoneNumber || "No number"
                            color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.7)
                            font.pixelSize: 13
                            font.family: fontFamily
                            elide: Text.ElideRight
                            width: parent.width
                        }

                        Text {
                            visible: model.email !== ""
                            text: model.email
                            color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.8)
                            font.pixelSize: 11
                            font.family: fontFamily
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }

                    // Call button
                    Rectangle {
                        width: 50
                        height: 50
                        anchors.verticalCenter: parent.verticalCenter
                        color: callMouseArea.pressed ? Qt.rgba(0, 1, 0, 0.3) :
                               callMouseArea.containsMouse ? Qt.rgba(0, 1, 0, 0.2) :
                               Qt.rgba(0, 1, 0, 0.1)
                        border.color: "#00ff00"
                        border.width: 1
                        radius: 25

                        Text {
                            anchors.centerIn: parent
                            text: "📞"
                            font.pixelSize: 24
                        }

                        MouseArea {
                            id: callMouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (contactManager) {
                                    contactManager.callContact(model.contactId)
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    id: contactMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    z: -1
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: bgCol

        Column {
            anchors.fill: parent
            spacing: 0

            // Header with search and sync button
            Rectangle {
                width: parent.width
                height: 60
                color: Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.5)

                Row {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    // Search field
                    Rectangle {
                        width: parent.width - 70
                        height: 40
                        anchors.verticalCenter: parent.verticalCenter
                        color: Qt.rgba(0, 0, 0, 0.3)
                        border.color: primaryCol
                        border.width: 1
                        radius: 4

                        TextInput {
                            id: searchField
                            anchors.fill: parent
                            anchors.margins: 10
                            anchors.rightMargin: searchField.text !== "" ? 35 : 10
                            color: textCol
                            font.pixelSize: 14
                            font.family: fontFamily
                            verticalAlignment: TextInput.AlignVCenter
                            selectByMouse: true

                            onTextChanged: {
                                filteredModel.updateFilter()
                            }
                        }

                        Text {
                            visible: searchField.text === ""
                            anchors.fill: parent
                            anchors.margins: 10
                            text: "Search contacts..."
                            color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.4)
                            font.pixelSize: 14
                            font.family: fontFamily
                            verticalAlignment: Text.AlignVCenter
                        }

                        // Clear button
                        Rectangle {
                            visible: searchField.text !== ""
                            width: 24
                            height: 24
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            color: clearMouseArea.pressed ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3) :
                                   clearMouseArea.containsMouse ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2) :
                                   "transparent"
                            radius: 12

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                color: primaryCol
                                font.pixelSize: 16
                                font.family: fontFamily
                            }

                            MouseArea {
                                id: clearMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    searchField.text = ""
                                }
                            }
                        }
                    }

                    // Sync button
                    Rectangle {
                        width: 60
                        height: 40
                        anchors.verticalCenter: parent.verticalCenter
                        color: contactManager && contactManager.isSyncing ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2) : "transparent"
                        border.color: primaryCol
                        border.width: 1
                        radius: 4

                        Text {
                            anchors.centerIn: parent
                            text: contactManager && contactManager.isSyncing ? "⟳" : "↻"
                            color: primaryCol
                            font.pixelSize: 24
                            font.family: fontFamily

                            RotationAnimation on rotation {
                                running: contactManager && contactManager.isSyncing
                                from: 0
                                to: 360
                                duration: 1000
                                loops: Animation.Infinite
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (contactManager && bluetoothManager) {
                                    var deviceAddress = bluetoothManager.getFirstConnectedDeviceAddress()
                                    if (deviceAddress !== "") {
                                        contactManager.syncContacts(deviceAddress)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Contact count and status
            Rectangle {
                width: parent.width
                height: 30
                color: "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        if (!contactManager) return "0 contacts"
                        var total = contactManager.contactCount
                        var filtered = contactList.count
                        if (searchField.text === "") {
                            return total + " contacts"
                        } else {
                            return filtered + " of " + total + " contacts"
                        }
                    }
                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.6)
                    font.pixelSize: 12
                    font.family: fontFamily
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 15
                    anchors.verticalCenter: parent.verticalCenter
                    text: contactManager ? contactManager.statusMessage : ""
                    color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.8)
                    font.pixelSize: 11
                    font.family: fontFamily
                }
            }

            // Contacts list
            ListView {
                id: contactList
                width: parent.width
                height: parent.height - 90
                clip: true

                model: filteredModel

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    width: 8

                    contentItem: Rectangle {
                        implicitWidth: 8
                        radius: 4
                        color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.5)
                    }
                }

                // Empty state
                Text {
                    visible: contactList.count === 0
                    anchors.centerIn: parent
                    text: {
                        if (contactManager && contactManager.isSyncing) {
                            return "Syncing contacts..."
                        } else if (searchField.text !== "") {
                            return "No contacts found\n\nTry a different search term"
                        } else {
                            return "No contacts\n\nClick the sync button to download\ncontacts from your phone"
                        }
                    }
                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                    font.pixelSize: 16
                    font.family: fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    lineHeight: 1.5
                }
            }
        }
    }

    // Auto-sync on load if we have a connected device
    Component.onCompleted: {
        if (contactManager && bluetoothManager && contactManager.contactCount === 0) {
            var deviceAddress = bluetoothManager.getFirstConnectedDeviceAddress()
            if (deviceAddress !== "") {
                Qt.callLater(function() {
                    contactManager.syncContacts(deviceAddress)
                })
            }
        }
    }
}
