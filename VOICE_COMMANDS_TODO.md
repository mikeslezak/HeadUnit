# Voice Commands Implementation - Continuation Guide

## Current Status

### ✅ Completed:
1. **VoiceCommandHandler class created** (`VoiceCommandHandler.h` and `VoiceCommandHandler.cpp`)
   - Parses Claude AI responses for JSON commands
   - Implements: call, message, read_messages actions
   - Includes confirmation flow for text messages
   - Uses ContactManager to lookup phone numbers by name
   - Uses BluetoothManager to make calls
   - Uses MessageManager to send texts
   - Uses GoogleTTS for voice feedback

2. **CMakeLists.txt updated** - VoiceCommandHandler added to build

3. **main.cpp updated** - VoiceCommandHandler include added (line 17)

4. **VoiceControl.qml updated** - Text input field added for testing without microphone (lines 62-100)

5. **Voice preference settings** - All TTS settings UI complete and functional

### ⚠️ What Needs to Be Done:

## Step 1: Complete main.cpp Integration

**File:** `/home/mike/HeadUnit/main.cpp`

**Location:** After line 65 (after `MessageManager messageManager;`)

**Add:**
```cpp
VoiceCommandHandler voiceCommandHandler;
```

**Location:** After line 95 (after context properties are set)

**Add:**
```cpp
// Set up VoiceCommandHandler dependencies
voiceCommandHandler.setContactManager(&contactManager);
voiceCommandHandler.setMessageManager(&messageManager);
voiceCommandHandler.setBluetoothManager(&bluetoothManager);
voiceCommandHandler.setGoogleTTS(&googleTTS);

// Expose to QML
engine.rootContext()->setContextProperty("voiceCommandHandler", &voiceCommandHandler);
```

## Step 2: Update ClaudeClient System Prompt

**File:** `/home/mike/HeadUnit/ClaudeClient.h` or `ClaudeClient.cpp`

**Find the system prompt** and add this instruction:

```
When the user requests a phone action (call, text, read messages), respond with a JSON command block in this format:

```json
{
  "action": "call|message|read_messages",
  "contact_name": "Contact Name",
  "message_body": "Message text here",
  "phone_number": "+1234567890"
}
```

Examples:
- "Call Mom" → {"action": "call", "contact_name": "Mom"}
- "Text John I'm running late" → {"action": "message", "contact_name": "John", "message_body": "I'm running late"}
- "Read messages from Sarah" → {"action": "read_messages", "contact_name": "Sarah"}

Always include the JSON block AND a natural language response.
```

## Step 3: Wire Up in Main.qml

**File:** `/home/mike/HeadUnit/Main.qml`

**Find:** The connection where `claudeClient.responseReceived` is handled

**Add:** Connection to process voice commands:

```qml
Connections {
    target: claudeClient
    function onResponseReceived(response) {
        // Process voice commands
        voiceCommandHandler.processClaudeResponse(response)

        // Existing response handling...
    }
}
```

**Add:** Connection for confirmation requests:

```qml
Connections {
    target: voiceCommandHandler

    function onConfirmationRequested(action, details) {
        // Show confirmation dialog
        confirmationDialog.action = action
        confirmationDialog.details = details
        confirmationDialog.visible = true
    }

    function onCommandExecuted(action, details) {
        console.log("Command executed:", action, details)
    }

    function onCommandFailed(action, error) {
        console.log("Command failed:", action, error)
    }
}
```

**Add:** Confirmation dialog component (somewhere in Main.qml):

```qml
// Voice command confirmation dialog
Rectangle {
    id: confirmationDialog
    anchors.centerIn: parent
    width: 500
    height: 200
    radius: 12
    color: Qt.rgba(0, 0, 0, 0.95)
    border.color: theme.palette.primary
    border.width: 2
    visible: false
    z: 1000

    property string action: ""
    property string details: ""

    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: confirmationDialog.details
            color: theme.palette.text
            font.pixelSize: 18
            font.family: theme.typography.fontFamily
            width: 450
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 20

            Rectangle {
                width: 120
                height: 50
                radius: 8
                color: "transparent"
                border.color: theme.palette.primary
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "Yes, Send"
                    color: theme.palette.primary
                    font.pixelSize: 16
                    font.family: theme.typography.fontFamily
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        voiceCommandHandler.confirmAction()
                        confirmationDialog.visible = false
                    }
                }
            }

            Rectangle {
                width: 120
                height: 50
                radius: 8
                color: "transparent"
                border.color: theme.palette.accent
                border.width: 2

                Text {
                    anchors.centerIn: parent
                    text: "Cancel"
                    color: theme.palette.accent
                    font.pixelSize: 16
                    font.family: theme.typography.fontFamily
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        voiceCommandHandler.cancelAction()
                        confirmationDialog.visible = false
                    }
                }
            }
        }
    }
}
```

## Step 4: Build and Test

```bash
cd /home/mike/HeadUnit/build
cmake --build . -j4
pkill -9 appHeadUnit
sleep 2
DISPLAY=:0 bash /home/mike/HeadUnit/launch_headunit.sh
```

## Step 5: Testing Commands

Open the voice control overlay and type these test commands:

1. **Test Call:** `call mom` or `call [contact name]`
2. **Test Text:** `text john I'm running late` or `send message to [name] saying [message]`
3. **Test Read:** `read messages from sarah` or `read my messages`

Watch the console output for:
- `VoiceCommandHandler: Processing Claude response...`
- `VoiceCommandHandler: Found JSON command:`
- `VoiceCommandHandler: Executing action:`

## Expected Behavior:

### For Calls:
1. User types: "call mom"
2. Claude responds with JSON command
3. VoiceCommandHandler looks up Mom's phone number in contacts
4. BluetoothManager dials the number
5. TTS says "Calling Mom"

### For Texts:
1. User types: "text john I'm running late"
2. Claude responds with JSON command
3. VoiceCommandHandler looks up John's phone number
4. Confirmation dialog appears
5. User clicks "Yes, Send"
6. Message is sent via MessageManager
7. TTS says "Message sent"

## Troubleshooting:

### If commands aren't being processed:
- Check console for "VoiceCommandHandler: No actionable command found"
- Verify Claude's system prompt includes JSON command instructions
- Check that claudeClient.responseReceived is connected to voiceCommandHandler

### If contact lookup fails:
- Verify ContactManager has synced contacts
- Check console for "VoiceCommandHandler: No contact found for: [name]"
- Try using exact contact name from phone

### If calls/texts don't work:
- Ensure Bluetooth is connected to phone
- Check BluetoothManager status
- Verify phone permissions for calls/texts

## Files Modified:
- `/home/mike/HeadUnit/VoiceCommandHandler.h` ✅ Created
- `/home/mike/HeadUnit/VoiceCommandHandler.cpp` ✅ Created
- `/home/mike/HeadUnit/CMakeLists.txt` ✅ Updated
- `/home/mike/HeadUnit/main.cpp` ⚠️ Partially updated (include added, need to finish)
- `/home/mike/HeadUnit/Ui/VoiceControl.qml` ✅ Updated (text input added)
- `/home/mike/HeadUnit/Main.qml` ⚠️ Needs connections added
- `/home/mike/HeadUnit/ClaudeClient.h` or `.cpp` ⚠️ Needs system prompt update

## Next Session Goals:
1. Complete main.cpp integration (5 minutes)
2. Update Claude system prompt (5 minutes)
3. Add connections in Main.qml (10 minutes)
4. Build and test (5 minutes)
5. Debug any issues (10 minutes)

Total estimated time: ~35 minutes to complete
