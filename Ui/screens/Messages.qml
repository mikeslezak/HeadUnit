import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    property var theme: null

    readonly property color textCol: theme?.palette?.text ?? "#39ff14"
    readonly property color primaryCol: theme?.palette?.primary ?? "#00f0ff"
    readonly property color bgCol: theme?.palette?.bg ?? "#0a0a0f"
    readonly property color cardBgCol: theme?.palette?.cardBg ?? "#1a1a1f"
    readonly property string fontFamily: theme?.typography?.fontFamily ?? "Noto Sans"
    readonly property int fontSize: theme?.typography?.fontSize ? Number(theme.typography.fontSize) : 16

    Rectangle {
        anchors.fill: parent
        color: bgCol

        // HORIZONTAL SPLIT LAYOUT: Conversation List | Message Thread
        Row {
            anchors.fill: parent
            spacing: 0

            // LEFT PANEL: Conversation List (400px)
            Rectangle {
                width: 400
                height: parent.height
                color: Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.3)
                border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                border.width: 1

                Column {
                    anchors.fill: parent
                    spacing: 0

                    // Header with title and sync button
                    Rectangle {
                        width: parent.width
                        height: 60
                        color: Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.6)
                        border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: "Messages"
                                color: primaryCol
                                font.pixelSize: fontSize + 6
                                font.family: fontFamily
                                font.weight: Font.Bold
                            }

                            Item { Layout.fillWidth: true; width: parent.width - 200 }

                            // Unread count badge
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: unreadText.width + 16
                                height: 24
                                color: Qt.rgba(1, 0, 0, 0.7)
                                border.color: "#ff0000"
                                border.width: 1
                                radius: 12
                                visible: messageManager.totalUnreadCount > 0

                                Text {
                                    id: unreadText
                                    anchors.centerIn: parent
                                    text: messageManager.totalUnreadCount
                                    color: textCol
                                    font.pixelSize: fontSize - 2
                                    font.family: fontFamily
                                    font.weight: Font.Bold
                                }
                            }

                            // Sync button
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 70
                                height: 36
                                color: syncMouseArea.pressed ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.4) :
                                       syncMouseArea.containsMouse ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3) :
                                       Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2)
                                border.color: primaryCol
                                border.width: 1
                                radius: 4

                                Text {
                                    anchors.centerIn: parent
                                    text: messageManager.isSyncing ? "⟳" : "↻"
                                    color: primaryCol
                                    font.pixelSize: fontSize + 4
                                    font.weight: Font.Bold

                                    RotationAnimation on rotation {
                                        running: messageManager.isSyncing
                                        loops: Animation.Infinite
                                        from: 0
                                        to: 360
                                        duration: 1000
                                    }
                                }

                                MouseArea {
                                    id: syncMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        if (!messageManager.isSyncing) {
                                            messageManager.syncMessages()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Conversation ListView
                    ListView {
                        id: conversationList
                        width: parent.width
                        height: parent.height - 60
                        clip: true
                        model: messageManager.conversationModel

                        delegate: Rectangle {
                            width: conversationList.width
                            height: 80
                            color: conversationMouseArea.pressed ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3) :
                                   conversationMouseArea.containsMouse ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.2) :
                                   messageManager.currentThreadId === model.threadId ? Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.25) :
                                   "transparent"
                            border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                            border.width: messageManager.currentThreadId === model.threadId ? 2 : 1

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Row {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12

                                // Avatar circle
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 50
                                    height: 50
                                    radius: 25
                                    color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                                    border.color: primaryCol
                                    border.width: 2

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.contactName ? model.contactName.charAt(0).toUpperCase() : "#"
                                        color: primaryCol
                                        font.pixelSize: fontSize + 6
                                        font.family: fontFamily
                                        font.weight: Font.Bold
                                    }
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width - 70
                                    spacing: 4

                                    Row {
                                        width: parent.width
                                        spacing: 8

                                        Text {
                                            text: model.contactName || model.contactAddress
                                            color: textCol
                                            font.pixelSize: fontSize + 2
                                            font.family: fontFamily
                                            font.weight: Font.Bold
                                            elide: Text.ElideRight
                                            width: parent.width - 80
                                        }

                                        Text {
                                            text: model.formattedTime || ""
                                            color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.6)
                                            font.pixelSize: fontSize - 3
                                            font.family: fontFamily
                                        }
                                    }

                                    Text {
                                        width: parent.width
                                        text: model.lastMessageBody || ""
                                        color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.7)
                                        font.pixelSize: fontSize - 1
                                        font.family: fontFamily
                                        elide: Text.ElideRight
                                        maximumLineCount: 2
                                        wrapMode: Text.WordWrap
                                    }

                                    // Unread count badge
                                    Rectangle {
                                        width: unreadConvText.width + 12
                                        height: 18
                                        color: Qt.rgba(1, 0, 0, 0.7)
                                        border.color: "#ff0000"
                                        border.width: 1
                                        radius: 9
                                        visible: model.unreadCount > 0

                                        Text {
                                            id: unreadConvText
                                            anchors.centerIn: parent
                                            text: model.unreadCount
                                            color: textCol
                                            font.pixelSize: fontSize - 4
                                            font.family: fontFamily
                                            font.weight: Font.Bold
                                        }
                                    }
                                }
                            }

                            MouseArea {
                                id: conversationMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    messageManager.loadConversation(model.threadId)
                                    messageManager.markAsRead(model.threadId)
                                }
                            }
                        }

                        // Empty state
                        Text {
                            visible: conversationList.count === 0 && !messageManager.isSyncing
                            anchors.centerIn: parent
                            text: messageManager.isConnected ? "No messages yet\n\nClick Sync to load messages" : "Not connected to phone\n\nPair a device via Bluetooth"
                            color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                            font.pixelSize: fontSize
                            font.family: fontFamily
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // RIGHT PANEL: Message Thread + Compose
            Rectangle {
                width: parent.width - 400
                height: parent.height
                color: "transparent"

                Column {
                    anchors.fill: parent
                    spacing: 0

                    // Thread Header
                    Rectangle {
                        width: parent.width
                        height: 60
                        color: Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.5)
                        border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                        border.width: 1
                        visible: messageManager.currentThreadId !== ""

                        Row {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 40
                                height: 40
                                radius: 20
                                color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                                border.color: primaryCol
                                border.width: 2

                                Text {
                                    anchors.centerIn: parent
                                    text: getCurrentConversationName().charAt(0).toUpperCase()
                                    color: primaryCol
                                    font.pixelSize: fontSize + 4
                                    font.family: fontFamily
                                    font.weight: Font.Bold
                                }
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                Text {
                                    text: getCurrentConversationName()
                                    color: textCol
                                    font.pixelSize: fontSize + 2
                                    font.family: fontFamily
                                    font.weight: Font.Bold
                                }

                                Text {
                                    text: getCurrentConversationAddress()
                                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.6)
                                    font.pixelSize: fontSize - 2
                                    font.family: fontFamily
                                }
                            }
                        }
                    }

                    // Messages ListView
                    ListView {
                        id: messageListView
                        width: parent.width
                        height: parent.height - (messageManager.currentThreadId !== "" ? 120 : 60)
                        clip: true
                        spacing: 8
                        model: messageManager.messageModel
                        verticalLayoutDirection: ListView.BottomToTop

                        delegate: Item {
                            width: messageListView.width
                            height: messageBubble.height + 16

                            Rectangle {
                                id: messageBubble
                                anchors.right: model.isIncoming ? undefined : parent.right
                                anchors.left: model.isIncoming ? parent.left : undefined
                                anchors.margins: 12
                                width: Math.min(messageText.implicitWidth + 24, parent.width * 0.7)
                                height: messageColumn.height + 16
                                color: model.isIncoming ? Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.7) :
                                                          Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                                border.color: model.isIncoming ? Qt.rgba(textCol.r, textCol.g, textCol.b, 0.3) : primaryCol
                                border.width: 1
                                radius: 8

                                Column {
                                    id: messageColumn
                                    anchors.centerIn: parent
                                    width: parent.width - 16
                                    spacing: 4

                                    Text {
                                        visible: model.isIncoming
                                        text: model.sender || ""
                                        color: primaryCol
                                        font.pixelSize: fontSize - 3
                                        font.family: fontFamily
                                        font.weight: Font.Bold
                                    }

                                    Text {
                                        id: messageText
                                        width: parent.width
                                        text: model.body || ""
                                        color: textCol
                                        font.pixelSize: fontSize
                                        font.family: fontFamily
                                        wrapMode: Text.WordWrap
                                    }

                                    Text {
                                        anchors.right: parent.right
                                        text: model.formattedTime || ""
                                        color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                                        font.pixelSize: fontSize - 4
                                        font.family: fontFamily
                                    }
                                }
                            }
                        }

                        // Empty thread state
                        Text {
                            visible: messageListView.count === 0 && messageManager.currentThreadId !== ""
                            anchors.centerIn: parent
                            text: "No messages in this conversation"
                            color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                            font.pixelSize: fontSize
                            font.family: fontFamily
                        }

                        // No conversation selected state
                        Column {
                            visible: messageManager.currentThreadId === ""
                            anchors.centerIn: parent
                            spacing: 20

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "💬"
                                font.pixelSize: 64
                                opacity: 0.3
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "Select a conversation to view messages"
                                color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.5)
                                font.pixelSize: fontSize + 2
                                font.family: fontFamily
                            }
                        }
                    }

                    // Compose Area
                    Rectangle {
                        width: parent.width
                        height: 60
                        color: Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.5)
                        border.color: Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.3)
                        border.width: 1
                        visible: messageManager.currentThreadId !== ""

                        Row {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            Rectangle {
                                width: parent.width - 60
                                height: parent.height
                                color: Qt.rgba(0, 0, 0, 0.5)
                                border.color: messageInput.activeFocus ? primaryCol : Qt.rgba(primaryCol.r, primaryCol.g, primaryCol.b, 0.5)
                                border.width: 2
                                radius: 6

                                TextInput {
                                    id: messageInput
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    color: textCol
                                    font.pixelSize: fontSize
                                    font.family: fontFamily
                                    clip: true
                                    verticalAlignment: TextInput.AlignVCenter

                                    Keys.onReturnPressed: {
                                        if (text.trim() !== "") {
                                            sendMessage()
                                        }
                                    }
                                }

                                Text {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    text: "Type a message..."
                                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.4)
                                    font.pixelSize: fontSize
                                    font.family: fontFamily
                                    visible: messageInput.text === ""
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            // Send button
                            Rectangle {
                                width: 52
                                height: parent.height
                                color: sendMouseArea.pressed ? Qt.rgba(0, 0.9, 0, 0.4) :
                                       sendMouseArea.containsMouse ? Qt.rgba(0, 0.9, 0, 0.3) :
                                       Qt.rgba(0, 0.8, 0, 0.25)
                                border.color: messageInput.text.trim() !== "" ? Qt.rgba(0, 1, 0, 0.8) : Qt.rgba(0, 1, 0, 0.4)
                                border.width: 2
                                radius: 6
                                enabled: messageInput.text.trim() !== ""

                                Text {
                                    anchors.centerIn: parent
                                    text: "▶"
                                    color: messageInput.text.trim() !== "" ? "#00ff00" : Qt.rgba(0, 1, 0, 0.4)
                                    font.pixelSize: fontSize + 6
                                    font.weight: Font.Bold
                                }

                                MouseArea {
                                    id: sendMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: messageInput.text.trim() !== ""
                                    onClicked: sendMessage()
                                }
                            }
                        }
                    }
                }
            }
        }

        // Connection status overlay
        Rectangle {
            visible: !messageManager.isConnected
            anchors.centerIn: parent
            width: 400
            height: 150
            color: Qt.rgba(cardBgCol.r, cardBgCol.g, cardBgCol.b, 0.95)
            border.color: primaryCol
            border.width: 2
            radius: 10

            Column {
                anchors.centerIn: parent
                spacing: 15

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "📱"
                    font.pixelSize: 48
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: messageManager.statusMessage || "Not connected to phone"
                    color: textCol
                    font.pixelSize: fontSize + 2
                    font.family: fontFamily
                    font.weight: Font.Bold
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Connect a phone via Bluetooth"
                    color: Qt.rgba(textCol.r, textCol.g, textCol.b, 0.7)
                    font.pixelSize: fontSize
                    font.family: fontFamily
                }
            }
        }
    }

    // Helper functions
    function getCurrentConversationName() {
        var conv = messageManager.conversationModel
        if (!conv) return ""

        for (var i = 0; i < conv.rowCount(); i++) {
            var threadId = conv.data(conv.index(i, 0), 0x0101) // ThreadIdRole
            if (threadId === messageManager.currentThreadId) {
                return conv.data(conv.index(i, 0), 0x0102) || conv.data(conv.index(i, 0), 0x0103) // ContactNameRole or ContactAddressRole
            }
        }
        return ""
    }

    function getCurrentConversationAddress() {
        var conv = messageManager.conversationModel
        if (!conv) return ""

        for (var i = 0; i < conv.rowCount(); i++) {
            var threadId = conv.data(conv.index(i, 0), 0x0101) // ThreadIdRole
            if (threadId === messageManager.currentThreadId) {
                return conv.data(conv.index(i, 0), 0x0103) || "" // ContactAddressRole
            }
        }
        return ""
    }

    function sendMessage() {
        var text = messageInput.text.trim()
        if (text !== "" && messageManager.currentThreadId !== "") {
            var address = getCurrentConversationAddress()
            messageManager.sendMessage(address, text)
            messageInput.text = ""
        }
    }

    Component.onCompleted: {
        console.log("Messages screen loaded")

        // Auto-connect to first paired device if not connected
        if (!messageManager.isConnected && bluetoothManager) {
            var connectedAddress = bluetoothManager.getFirstConnectedDeviceAddress()
            if (connectedAddress !== "") {
                console.log("Messages: Auto-connecting to device:", connectedAddress)
                messageManager.connectToDevice(connectedAddress)
            } else {
                console.log("Messages: No connected Bluetooth device found")
            }
        }
    }
}
