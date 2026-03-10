import QtQuick 2.15
import HeadUnit

/**
 * VoicePipeline - Encapsulates all voice assistant signal wiring.
 *
 * Manages connections between PicovoiceManager, GoogleTTS, ClaudeClient,
 * and VoiceCommandHandler, plus the Claude indicator UI state and
 * voice confirmation dialog.
 */
Item {
    id: root
    anchors.fill: parent

    // Loader references from Main.qml
    property var claudeIndicatorLoader: null
    property var voiceControlLoader: null

    // Theme for confirmation dialog
    property var theme: null

    // Signal to show notifications in Main.qml
    signal notificationRequested(string message, string type)

    // PicovoiceManager Connections - Unified Voice Pipeline
    Connections {
        target: picovoiceManager

        function onWakeWordDetected(keyword) {
            console.log("Wake word detected via PicovoiceManager:", keyword, "- Activating Claude AI")

            // CRITICAL: Stop any ongoing TTS playback immediately to prevent audio conflicts
            if (googleTTS.isSpeaking) {
                console.log("Stopping TTS playback before processing new wake word")
                googleTTS.stop()
            }

            // Cancel any pending hide timer
            hideClaudeTimer.stop()

            // Activate Claude indicator (same as long-press home button)
            if (claudeIndicatorLoader) {
                claudeIndicatorLoader.active = true

                if (claudeIndicatorLoader.item) {
                    claudeIndicatorLoader.item.show()
                    claudeIndicatorLoader.item.setState("listening")
                } else {
                    console.warn("ClaudeIndicator not loaded yet after setting active=true")
                }
            }

            // Play a short "ready" voice prompt to indicate we're listening
            var readyPhrases = ["Yes?", "What's up?", "I'm here", "Go ahead", "Listening"]
            var randomIndex = Math.floor(Math.random() * readyPhrases.length)
            googleTTS.speak(readyPhrases[randomIndex])
        }

        function onIntentDetected(intent, slots) {
            console.log("Intent detected:", intent, "Slots:", JSON.stringify(slots))

            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.setState("processing")
            }

            // Auto-hide after timeout for quick commands
            hideClaudeTimer.start()
        }

        function onTranscriptionReady(text) {
            console.log("Speech transcription from Leopard:", text)

            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.setState("processing")
            }
        }

        function onError(message) {
            console.log("PicovoiceManager error:", message)
            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.hide()
            }
            root.notificationRequested("Voice Error: " + message, "error")
        }
    }

    // Google TTS Connections - for ready prompt completion
    Connections {
        target: googleTTS

        function onSpeechFinished() {
            console.log("TTS speech finished, notifying PicovoiceManager")
            picovoiceManager.onReadyPromptFinished()

            hideClaudeTimer.stop()
            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.hide()
            }
        }
    }

    // Claude Client Connections
    Connections {
        target: claudeClient

        function onProcessingChanged() {
            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                if (claudeClient.isProcessing) {
                    claudeIndicatorLoader.item.setState("processing")
                } else {
                    hideClaudeTimer.start()
                }
            }
        }

        function onResponseReceived(response, toolCalls) {
            console.log("Claude response:", response)
            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.setState("speaking")
            }

            voiceCommandHandler.processClaudeResponse(response)
            googleTTS.speak(response)

            // Fallback: hide after 30s in case TTS fails silently
            hideClaudeTimer.start()
        }

        function onError(message) {
            console.log("Claude error:", message)
            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.hide()
            }
            root.notificationRequested("Claude Error: " + message, "error")
        }
    }

    // Voice Command Handler Connections
    Connections {
        target: voiceCommandHandler

        function onConfirmationRequested(action, details) {
            console.log("Voice command confirmation requested:", action, details)
            voiceConfirmationDialog.action = action
            voiceConfirmationDialog.details = details
            voiceConfirmationDialog.visible = true
        }

        function onCommandExecuted(action, details) {
            console.log("Voice command executed:", action, details)
        }

        function onCommandFailed(action, error) {
            console.log("Voice command failed:", action, error)
            root.notificationRequested("Command failed: " + error, "error")
        }
    }

    // Fallback timer to hide Claude indicator if TTS speechFinished doesn't fire
    Timer {
        id: hideClaudeTimer
        interval: 30000
        repeat: false
        onTriggered: {
            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.hide()
            }
        }
    }

    // Voice Command Confirmation Dialog
    Rectangle {
        id: voiceConfirmationDialog
        anchors.centerIn: parent
        width: 500
        height: 200
        radius: 12
        color: Qt.rgba(0, 0, 0, 0.95)
        border.color: ThemeValues.primaryCol
        border.width: 2
        visible: false
        z: 1002

        property string action: ""
        property string details: ""

        Column {
            anchors.centerIn: parent
            spacing: 20

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: voiceConfirmationDialog.details
                color: ThemeValues.textCol
                font.pixelSize: 18
                font.family: ThemeValues.fontFamily
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
                    border.color: ThemeValues.primaryCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "Yes, Send"
                        color: ThemeValues.primaryCol
                        font.pixelSize: 16
                        font.family: ThemeValues.fontFamily
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            voiceCommandHandler.confirmAction()
                            voiceConfirmationDialog.visible = false
                        }
                    }
                }

                Rectangle {
                    width: 120
                    height: 50
                    radius: 8
                    color: "transparent"
                    border.color: ThemeValues.accentCol
                    border.width: 2

                    Text {
                        anchors.centerIn: parent
                        text: "Cancel"
                        color: ThemeValues.accentCol
                        font.pixelSize: 16
                        font.family: ThemeValues.fontFamily
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            voiceCommandHandler.cancelAction()
                            voiceConfirmationDialog.visible = false
                        }
                    }
                }
            }
        }
    }

    // Manual activation support (called from NavBar long-press)
    function activate() {
        console.log("Claude AI activated via manual trigger")

        if (claudeIndicatorLoader) {
            claudeIndicatorLoader.active = true

            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.show()
                claudeIndicatorLoader.item.setState("listening")
            }
        }

        picovoiceManager.manualActivate()
    }
}
