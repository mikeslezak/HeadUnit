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

    // Track which TTS speech is playing to handle onSpeechFinished correctly
    // "ready" = ready prompt after wake word, "response" = Claude's response, "alert" = copilot alert
    property string pendingSpeechType: ""

    // Whether Claude's last response expects a follow-up reply
    property bool pendingFollowUp: false

    // ScreenContainer reference for reading GPS/route data
    property var screenContainer: null

    // Track which music source was playing when Jarvis activated, so we can resume after
    property string pausedAudioSource: ""

    // Signals to Main.qml
    signal notificationRequested(string message, string type)
    signal navigateToDestination(string destination)

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

            // Pause any playing music so Jarvis can be heard
            root.pausedAudioSource = ""
            console.log("VoicePipeline: Checking music state - tidal:", tidalClient ? tidalClient.isPlaying : "null",
                        "spotify:", spotifyClient ? spotifyClient.isPlaying : "null",
                        "media:", mediaController ? mediaController.isPlaying : "null")
            if (tidalClient && tidalClient.isPlaying) {
                console.log("VoicePipeline: Pausing Tidal")
                tidalClient.pause()
                root.pausedAudioSource = "tidal"
            } else if (spotifyClient && spotifyClient.isPlaying) {
                console.log("VoicePipeline: Pausing Spotify")
                spotifyClient.pause()
                root.pausedAudioSource = "spotify"
            } else if (mediaController && mediaController.isPlaying) {
                console.log("VoicePipeline: Pausing MediaController")
                mediaController.pause()
                root.pausedAudioSource = "music"
            } else {
                console.log("VoicePipeline: No music source detected as playing")
            }

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
            root.pendingSpeechType = "ready"
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

    // Google TTS Connections - for ready prompt and response completion
    Connections {
        target: googleTTS

        function onSpeechStarted() {
            // Pause wake word detection while TTS is playing to prevent
            // the speaker output from triggering false "jarvis" detections
            picovoiceManager.pause()
        }

        function onSpeechFinished() {
            if (root.pendingSpeechType === "ready") {
                // Ready prompt finished — tell PicovoiceManager to start listening
                console.log("TTS ready prompt finished, starting command listening")
                picovoiceManager.resume()
                picovoiceManager.onReadyPromptFinished()

                // Update indicator to show we're listening
                if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                    claudeIndicatorLoader.item.setState("listening")
                }
            } else if (root.pendingSpeechType === "response") {
                // Claude response finished speaking
                hideClaudeTimer.stop()

                if (root.pendingFollowUp) {
                    // Follow-up expected — keep indicator visible, enter follow-up mode
                    console.log("TTS response finished, entering follow-up mode")
                    root.pendingFollowUp = false
                    if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                        claudeIndicatorLoader.item.setState("listening")
                    }
                    // PicovoiceManager.enterFollowUpMode() is called via C++ signal
                    // Just resume audio processing after echo delay
                    resumeAfterTTSTimer.start()
                } else {
                    // No follow-up — hide indicator, resume wake word and music
                    console.log("TTS response finished, hiding Claude indicator")
                    if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                        claudeIndicatorLoader.item.hide()
                    }
                    resumeMusic()
                    resumeAfterTTSTimer.start()
                }
            } else {
                // Unknown speech finished — just resume listening
                picovoiceManager.resume()
            }
            root.pendingSpeechType = ""
        }
    }

    // Brief delay before resuming wake word detection after TTS response
    // to avoid picking up tail-end speaker echo
    Timer {
        id: resumeAfterTTSTimer
        interval: 500
        repeat: false
        onTriggered: {
            picovoiceManager.resume()
        }
    }

    // Claude Client Connections
    Connections {
        target: claudeClient

        function onProcessingChanged() {
            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                if (claudeClient.isProcessing) {
                    claudeIndicatorLoader.item.setState("processing")
                }
                // Don't start hide timer here — onSpeechFinished handles it after TTS completes
            }
        }

        function onResponseReceived(response, toolCalls) {
            console.log("Claude response:", response)
            if (claudeIndicatorLoader && claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.setState("speaking")
            }

            voiceCommandHandler.processClaudeResponse(response)

            // Check for expects_reply in JSON command block
            root.pendingFollowUp = false
            var jsonMatch = response.match(/```json\s*(\{[\s\S]*?\})\s*```/)
            if (jsonMatch) {
                try {
                    var cmd = JSON.parse(jsonMatch[1])
                    if (cmd.expects_reply === true) {
                        root.pendingFollowUp = true
                    }
                } catch(e) {}
            }

            // Strip JSON command blocks before speaking — TTS should only read natural language
            var spokenText = response.replace(/```json[\s\S]*?```/g, "").trim()
            if (spokenText.length === 0) spokenText = "Done."

            root.pendingSpeechType = "response"
            googleTTS.speak(spokenText)

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

        function onNavigationRequested(destination) {
            console.log("Voice navigation requested:", destination)
            root.navigateToDestination(destination)
        }

        function onCommandFailed(action, error) {
            console.log("Voice command failed:", action, error)
            root.notificationRequested("Command failed: " + error, "error")
        }
    }

    // CopilotMonitor proactive alerts — speak them via TTS
    Connections {
        target: copilotMonitor

        function onProactiveAlert(message) {
            console.log("Copilot proactive alert:", message)

            // Pause music for alert
            root.pausedAudioSource = ""
            if (tidalClient && tidalClient.isPlaying) {
                tidalClient.pause()
                root.pausedAudioSource = "tidal"
            } else if (spotifyClient && spotifyClient.isPlaying) {
                spotifyClient.pause()
                root.pausedAudioSource = "spotify"
            } else if (mediaController && mediaController.isPlaying) {
                mediaController.pause()
                root.pausedAudioSource = "music"
            }

            // Show Claude indicator for alert
            if (claudeIndicatorLoader) {
                claudeIndicatorLoader.active = true
                if (claudeIndicatorLoader.item) {
                    claudeIndicatorLoader.item.show()
                    claudeIndicatorLoader.item.setState("speaking")
                }
            }

            root.pendingSpeechType = "response"
            root.pendingFollowUp = false
            googleTTS.speak(message)
            hideClaudeTimer.start()
        }
    }

    // Forward route state from ScreenContainer to ContextAggregator
    Timer {
        id: routeForwardTimer
        interval: 5000
        repeat: true
        running: true
        onTriggered: {
            if (root.screenContainer && root.screenContainer.navActive !== undefined) {
                contextAggregator.routeActive = root.screenContainer.navActive
                contextAggregator.routeDuration = root.screenContainer.navRouteDuration || ""
            }
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
            resumeMusic()
        }
    }

    // Voice Command Confirmation Dialog
    Rectangle {
        id: voiceConfirmationDialog
        anchors.centerIn: parent
        width: 500
        height: 200
        radius: 12
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.95)
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

    // Resume music that was paused when Jarvis activated
    function resumeMusic() {
        if (root.pausedAudioSource === "tidal" && tidalClient) {
            tidalClient.play()
        } else if (root.pausedAudioSource === "spotify" && spotifyClient) {
            spotifyClient.play()
        } else if (root.pausedAudioSource === "music" && mediaController) {
            mediaController.play()
        }
        root.pausedAudioSource = ""
    }

    // Manual activation support (called from NavBar long-press)
    function activate() {
        console.log("Claude AI activated via manual trigger")

        // Pause music
        root.pausedAudioSource = ""
        if (tidalClient && tidalClient.isPlaying) {
            tidalClient.pause()
            root.pausedAudioSource = "tidal"
        } else if (spotifyClient && spotifyClient.isPlaying) {
            spotifyClient.pause()
            root.pausedAudioSource = "spotify"
        } else if (mediaController && mediaController.isPlaying) {
            mediaController.pause()
            root.pausedAudioSource = "music"
        }

        if (claudeIndicatorLoader) {
            claudeIndicatorLoader.active = true

            if (claudeIndicatorLoader.item) {
                claudeIndicatorLoader.item.show()
                claudeIndicatorLoader.item.setState("listening")
            }
        }

        picovoiceManager.manualActivate()
    }

    // Cancel current voice interaction (called when user taps to dismiss)
    function cancelInteraction() {
        console.log("VoicePipeline: Canceling interaction, resuming music")
        root.pendingSpeechType = ""
        root.pendingFollowUp = false
        hideClaudeTimer.stop()
        resumeMusic()
    }
}
