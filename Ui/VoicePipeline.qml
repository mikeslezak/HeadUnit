import QtQuick 2.15
import HeadUnit

/**
 * VoicePipeline — Voice assistant orchestration layer.
 *
 * Wires PicovoiceManager, GoogleTTS, ClaudeClient, ToolExecutor, and CopilotMonitor.
 * ClaudeClient handles the tool loop internally; this layer manages:
 *   - TTS playback sequencing (ready prompt → response → follow-up / nav briefing)
 *   - Music pause/resume around voice interactions
 *   - Proactive alert queuing and delivery
 *   - Navigation briefing coordination (short ack → wait for route data → full briefing)
 */
Item {
    id: root
    anchors.fill: parent

    // --- External references ---
    property var claudeIndicatorLoader: null
    property var screenContainer: null

    // --- Interaction state ---
    // What the current/last TTS utterance represents:
    //   "" = idle, "ready" = wake word prompt, "response" = Claude response
    property string speechType: ""

    // Should the mic stay open after the current response? (set by ToolExecutor)
    property bool wantsFollowUp: false

    // Which music source was playing before Jarvis activated (for resume)
    property string musicSource: ""

    // --- Navigation briefing state ---
    // Destination waiting for route data (set when navigate tool fires, cleared when briefing sends)
    property string navDest: ""
    // True from navigate tool until the briefing TTS finishes (prevents premature cleanup)
    property bool navActive: false

    // --- Alert queue ---
    property var alertQueue: []

    // --- Signals to Main.qml ---
    signal notificationRequested(string message, string type)
    signal navigateToDestination(string destination)
    signal addRouteStop(string destination)
    signal cancelNavigation()

    // =====================================================================
    // INDICATOR HELPERS
    // =====================================================================
    function showIndicator(state) {
        if (!claudeIndicatorLoader) return
        claudeIndicatorLoader.active = true
        if (claudeIndicatorLoader.item) {
            claudeIndicatorLoader.item.show()
            claudeIndicatorLoader.item.setState(state)
        }
    }
    function setIndicator(state) {
        if (claudeIndicatorLoader && claudeIndicatorLoader.item)
            claudeIndicatorLoader.item.setState(state)
    }
    function hideIndicator() {
        if (claudeIndicatorLoader && claudeIndicatorLoader.item)
            claudeIndicatorLoader.item.hide()
    }

    // =====================================================================
    // MUSIC PAUSE / RESUME
    // =====================================================================
    function pauseMusic() {
        root.musicSource = ""
        if (tidalClient && tidalClient.isPlaying) {
            tidalClient.pause()
            root.musicSource = "tidal"
        } else if (spotifyClient && spotifyClient.isPlaying) {
            spotifyClient.pause()
            root.musicSource = "spotify"
        } else if (mediaController && mediaController.isPlaying) {
            mediaController.pause()
            root.musicSource = "bluetooth"
        }
    }

    function resumeMusic() {
        if (root.musicSource === "tidal" && tidalClient) tidalClient.play()
        else if (root.musicSource === "spotify" && spotifyClient) spotifyClient.play()
        else if (root.musicSource === "bluetooth" && mediaController) mediaController.play()
        root.musicSource = ""
    }

    // =====================================================================
    // TEARDOWN — end an interaction cleanly
    // =====================================================================
    function finishInteraction() {
        console.log("VoicePipeline: Finishing interaction")
        hideIndicator()
        resumeMusic()
        resumeAfterTTSTimer.start()
    }

    // =====================================================================
    // PICOVOICE CONNECTIONS
    // =====================================================================
    Connections {
        target: picovoiceManager

        function onWakeWordDetected(keyword) {
            console.log("Wake word detected:", keyword)

            // Dismiss any pending confirmation dialog
            if (voiceConfirmationDialog.visible) {
                toolExecutor.cancelAction()
                voiceConfirmationDialog.visible = false
            }

            // Cancel any in-progress nav briefing or alerts
            navBriefingTimer.stop()
            root.navDest = ""
            root.navActive = false
            root.alertQueue = []
            root.wantsFollowUp = false
            hideClaudeTimer.stop()

            // Stop any ongoing TTS
            if (googleTTS.isSpeaking) googleTTS.stop()

            pauseMusic()
            showIndicator("listening")

            // Play random ready prompt
            root.speechType = "ready"
            var phrases = ["Yes?", "What's up?", "I'm here", "Go ahead", "Listening"]
            googleTTS.speak(phrases[Math.floor(Math.random() * phrases.length)])
        }

        function onTranscriptionReady(text) {
            console.log("Transcription:", text)
            setIndicator("processing")
        }

        function onIntentDetected(intent, slots) {
            console.log("Intent:", intent, JSON.stringify(slots))
            setIndicator("processing")
            hideClaudeTimer.start()
        }

        function onInteractionReset() {
            // PicovoiceManager silently returned to Listening (follow-up timeout, speech start timeout, etc.)
            console.log("VoicePipeline: PicovoiceManager interaction reset")
            root.speechType = ""
            root.wantsFollowUp = false
            hideClaudeTimer.stop()
            hideIndicator()
            resumeMusic()
        }

        function onError(message) {
            console.log("PicovoiceManager error:", message)
            root.speechType = ""
            root.wantsFollowUp = false
            root.navDest = ""
            root.navActive = false
            root.alertQueue = []
            navBriefingTimer.stop()
            hideClaudeTimer.stop()
            hideIndicator()
            resumeMusic()
            root.notificationRequested("Voice Error: " + message, "error")
        }
    }

    // =====================================================================
    // GOOGLE TTS CONNECTIONS
    // =====================================================================
    Connections {
        target: googleTTS

        function onSpeechStarted() {
            picovoiceManager.pause()
            hideClaudeTimer.stop()
        }

        function onError(message) {
            console.log("GoogleTTS error:", message)
            // TTS failed — speechFinished will never fire, so clean up manually
            var type = root.speechType
            root.speechType = ""
            root.wantsFollowUp = false
            hideClaudeTimer.stop()

            if (type === "ready") {
                // Ready prompt failed — cancel the whole interaction
                root.navDest = ""
                root.navActive = false
                root.alertQueue = []
                navBriefingTimer.stop()
                picovoiceManager.cancelAndReset()
                hideIndicator()
                resumeMusic()
            } else if (type === "response") {
                // Response TTS failed — same cleanup as finishInteraction
                root.navActive = false
                root.alertQueue = []
                navBriefingTimer.stop()
                picovoiceManager.resume()
                finishInteraction()
            } else {
                picovoiceManager.resume()
            }
        }

        function onSpeechFinished() {
            var type = root.speechType
            root.speechType = ""

            if (type === "ready") {
                // Ready prompt done — start listening for user's command
                console.log("Ready prompt done, listening for command")
                picovoiceManager.resume()
                picovoiceManager.onReadyPromptFinished()
                setIndicator("listening")

            } else if (type === "response") {
                hideClaudeTimer.stop()

                if (root.navDest !== "") {
                    // This was the short nav ack ("On it") — wait for briefing timer
                    console.log("Nav ack done, waiting for route briefing...")
                    setIndicator("thinking")
                    picovoiceManager.resume()

                } else if (root.navActive) {
                    // This was the full nav briefing — wrap up the whole nav flow
                    console.log("Nav briefing done, finishing")
                    root.navActive = false
                    root.alertQueue = []
                    finishInteraction()

                } else if (root.alertQueue.length > 0) {
                    // Queued proactive alerts — dispatch them
                    var alerts = root.alertQueue
                    root.alertQueue = []
                    setIndicator("thinking")
                    var prompt = "New conditions just came in. Mention these naturally and concisely: " + alerts.join(". ")
                    claudeClient.sendMessage(prompt, contextAggregator.buildContext(), true)

                } else if (root.wantsFollowUp) {
                    // Claude asked a question — keep mic open
                    console.log("Entering follow-up mode")
                    root.wantsFollowUp = false
                    setIndicator("listening")
                    picovoiceManager.enterFollowUpMode()
                    resumeAfterTTSTimer.start()

                } else {
                    // Normal response — done
                    console.log("Response done, finishing")
                    finishInteraction()
                }

            } else {
                // Unknown speech type — resume mic and check for queued alerts
                picovoiceManager.resume()
                if (root.alertQueue.length > 0) {
                    var pendingAlerts = root.alertQueue
                    root.alertQueue = []
                    speakAlert(pendingAlerts.join(". "))
                }
            }
        }
    }

    Timer {
        id: resumeAfterTTSTimer
        interval: 400
        repeat: false
        onTriggered: picovoiceManager.resume()
    }

    // =====================================================================
    // CLAUDE CLIENT CONNECTIONS
    // =====================================================================
    Connections {
        target: claudeClient

        function onProcessingChanged() {
            if (claudeClient.isProcessing) setIndicator("processing")
        }

        function onResponseReceived(response, toolCalls) {
            console.log("Claude response:", response.substring(0, 200))
            setIndicator("speaking")

            var spokenText = stripMarkdown(response.trim())
            if (spokenText.length === 0) spokenText = "Done."

            root.speechType = "response"
            googleTTS.speak(spokenText)
            hideClaudeTimer.start()
        }

        function onError(message) {
            console.log("Claude error:", message)
            root.speechType = ""
            navBriefingTimer.stop()
            hideClaudeTimer.stop()
            root.navDest = ""
            root.navActive = false
            root.alertQueue = []
            root.wantsFollowUp = false
            if (voiceConfirmationDialog.visible) {
                voiceConfirmationDialog.visible = false
            }
            hideIndicator()
            picovoiceManager.resume()
            resumeMusic()
            root.notificationRequested("Claude Error: " + message, "error")
        }
    }

    // =====================================================================
    // TOOL EXECUTOR CONNECTIONS
    // =====================================================================
    Connections {
        target: toolExecutor

        function onNavigationStarted(destination) {
            console.log("Navigation started to:", destination)
            root.navDest = destination
            root.navActive = true
            navBriefingTimer.start()
            root.navigateToDestination(destination)
        }

        function onRouteStopRequested(destination) {
            console.log("Route stop requested:", destination)
            root.addRouteStop(destination)
        }

        function onRouteCancelled() {
            console.log("Route cancelled by voice command")
            root.navDest = ""
            root.navActive = false
            root.cancelNavigation()
        }

        function onFollowUpExpected() {
            console.log("Follow-up expected")
            root.wantsFollowUp = true
        }

        function onConfirmationRequested(action, details) {
            console.log("Confirmation requested:", action, details)
            voiceConfirmationDialog.action = action
            voiceConfirmationDialog.details = details
            voiceConfirmationDialog.visible = true
        }
    }

    // =====================================================================
    // COPILOT MONITOR — proactive alerts
    // =====================================================================
    Connections {
        target: copilotMonitor

        function onProactiveAlert(message) {
            console.log("Proactive alert:", message)

            // If nav briefing is pending, collect alerts for the unified briefing
            if (root.navDest !== "") {
                var q = root.alertQueue
                q.push(message)
                root.alertQueue = q
                navBriefingTimer.restart()
                return
            }

            // If TTS is busy, queue for later
            if (googleTTS.isSpeaking || root.speechType !== "") {
                var q2 = root.alertQueue
                q2.push(message)
                root.alertQueue = q2
                return
            }

            // Speak it now
            speakAlert(message)
        }
    }

    // =====================================================================
    // NAV BRIEFING — unified route briefing after route data arrives
    // =====================================================================
    function deliverNavBriefing() {
        var destination = root.navDest
        var alerts = root.alertQueue

        root.navDest = ""        // Clear so onSpeechFinished knows this is the briefing
        root.alertQueue = []
        // root.navActive stays true until the briefing TTS finishes

        var prompt = "You just started navigation to " + destination + ". "
        if (alerts.length > 0) {
            prompt += "Here is the route data that just came in: " + alerts.join(" ") + " "
        }
        prompt += "Give the driver a single, cohesive route briefing. Acknowledge the destination, mention the drive time if you know it, then cover weather and road conditions in one natural flow. Keep it concise."

        console.log("Delivering nav briefing for", destination, "with", alerts.length, "alerts")

        showIndicator("processing")
        root.wantsFollowUp = false
        root.speechType = ""
        claudeClient.sendMessage(prompt, contextAggregator.buildContext(), true)
        hideClaudeTimer.start()
    }

    function speakAlert(message) {
        if (root.musicSource === "") pauseMusic()

        showIndicator("thinking")
        root.wantsFollowUp = false
        var prompt = "New route conditions detected. Mention these naturally and concisely: " + message
        claudeClient.sendMessage(prompt, contextAggregator.buildContext(), true)
        hideClaudeTimer.start()
    }

    // =====================================================================
    // TIMERS
    // =====================================================================

    // Wait for route data before sending nav briefing to Claude
    Timer {
        id: navBriefingTimer
        interval: 8000
        repeat: false
        onTriggered: deliverNavBriefing()
    }

    // Fallback: hide indicator if TTS speechFinished never fires
    Timer {
        id: hideClaudeTimer
        interval: 30000
        repeat: false
        onTriggered: {
            console.log("hideClaudeTimer fallback fired")
            // Reset ALL interaction state to prevent stale state corrupting next interaction
            root.speechType = ""
            root.wantsFollowUp = false
            root.navDest = ""
            root.navActive = false
            root.alertQueue = []
            navBriefingTimer.stop()
            googleTTS.stop()
            picovoiceManager.resume()
            hideIndicator()
            resumeMusic()
        }
    }

    // Forward route state to ContextAggregator
    Timer {
        id: routeForwardTimer
        interval: 5000
        repeat: true
        running: true
        onTriggered: {
            if (root.screenContainer && root.screenContainer.navActive !== undefined) {
                contextAggregator.routeActive = root.screenContainer.navActive
                contextAggregator.routeDuration = root.screenContainer.navRouteDuration || ""
                contextAggregator.routeDistance = root.screenContainer.navRouteDistance || ""
                contextAggregator.routeDestination = root.screenContainer.navRouteDestinationName || ""
            }
        }
    }

    // =====================================================================
    // UTILITY
    // =====================================================================

    function stripMarkdown(text) {
        text = text.replace(/\*\*([^*]+)\*\*/g, "$1")
        text = text.replace(/\*([^*]+)\*/g, "$1")
        text = text.replace(/__([^_]+)__/g, "$1")
        text = text.replace(/_([^_]+)_/g, "$1")
        text = text.replace(/^#{1,6}\s+/gm, "")
        text = text.replace(/^[\s]*[-*+]\s+/gm, "")
        text = text.replace(/^[\s]*\d+\.\s+/gm, "")
        text = text.replace(/`([^`]+)`/g, "$1")
        text = text.replace(/```[\s\S]*?```/g, "")
        text = text.replace(/\n{3,}/g, "\n\n")
        return text.trim()
    }

    // Manual activation (NavBar long-press)
    // NOTE: manualActivate() emits wakeWordDetected which triggers onWakeWordDetected.
    // Do NOT call pauseMusic() here — onWakeWordDetected already does it.
    // Calling it twice clobbers musicSource to "" so music never resumes.
    function activate() {
        console.log("Manual voice activation")
        picovoiceManager.manualActivate()
    }

    // Cancel current interaction
    function cancelInteraction() {
        console.log("Canceling interaction")
        root.speechType = ""
        root.wantsFollowUp = false
        root.navDest = ""
        root.navActive = false
        root.alertQueue = []
        navBriefingTimer.stop()
        hideClaudeTimer.stop()
        googleTTS.stop()
        picovoiceManager.cancelAndReset()
        hideIndicator()
        resumeMusic()
    }

    // =====================================================================
    // CONFIRMATION DIALOG (SMS)
    // =====================================================================
    Timer {
        id: confirmationDismissTimer
        interval: 30000
        repeat: false
        onTriggered: {
            if (voiceConfirmationDialog.visible) {
                console.log("VoicePipeline: Confirmation dialog auto-dismissed after 30s")
                toolExecutor.cancelAction()
                voiceConfirmationDialog.visible = false
            }
        }
    }

    Rectangle {
        id: voiceConfirmationDialog
        anchors.centerIn: parent
        width: 500; height: 200; radius: 12
        color: Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.95)
        border.color: ThemeValues.primaryCol; border.width: 2
        visible: false; z: 1002
        onVisibleChanged: {
            if (visible) confirmationDismissTimer.start()
            else confirmationDismissTimer.stop()
        }

        property string action: ""
        property string details: ""

        Column {
            anchors.centerIn: parent; spacing: 20

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: voiceConfirmationDialog.details
                color: ThemeValues.textCol
                font.pixelSize: 18; font.family: ThemeValues.fontFamily
                width: 450; wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter; spacing: 20

                Rectangle {
                    width: 120; height: 50; radius: 8
                    color: "transparent"
                    border.color: ThemeValues.primaryCol; border.width: 2
                    Text {
                        anchors.centerIn: parent; text: "Yes, Send"
                        color: ThemeValues.primaryCol
                        font.pixelSize: 16; font.family: ThemeValues.fontFamily
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { toolExecutor.confirmAction(); voiceConfirmationDialog.visible = false }
                    }
                }

                Rectangle {
                    width: 120; height: 50; radius: 8
                    color: "transparent"
                    border.color: ThemeValues.accentCol; border.width: 2
                    Text {
                        anchors.centerIn: parent; text: "Cancel"
                        color: ThemeValues.accentCol
                        font.pixelSize: 16; font.family: ThemeValues.fontFamily
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { toolExecutor.cancelAction(); voiceConfirmationDialog.visible = false }
                    }
                }
            }
        }
    }
}
