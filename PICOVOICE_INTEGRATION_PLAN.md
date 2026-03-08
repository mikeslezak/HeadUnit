# Picovoice Integration Plan for HeadUnit

## Overview
This document outlines the integration of Picovoice's voice recognition suite (Porcupine + Leopard + Rhino + Koala) to replace Google STT and create a unified, offline audio pipeline for the HeadUnit project.

## Completed Steps

### 1. Downloaded Picovoice SDKs ✓
All three additional SDKs have been downloaded and installed for ARM64 (Cortex-A76, compatible with Jetson Orin Nano):

**Leopard (Speech-to-Text)**
- Location: `/home/mike/HeadUnit/external/leopard/`
- Library: `lib/libpv_leopard.so` (1.3MB)
- Model: `common/leopard_params.pv` (36MB, English)
- Headers: `include/pv_leopard.h`, `include/picovoice.h`

**Rhino (Speech-to-Intent)**
- Location: `/home/mike/HeadUnit/external/rhino/`
- Library: `lib/libpv_rhino.so` (156KB)
- Model: `common/rhino_params.pv` (2.1MB, English)
- Headers: `include/pv_rhino.h`, `include/picovoice.h`

**Koala (Noise Suppression)**
- Location: `/home/mike/HeadUnit/external/koala/`
- Library: `lib/libpv_koala.so` (140KB)
- Headers: `include/pv_koala.h`, `include/picovoice.h`

**Porcupine (Wake Word Detection)** - Already integrated
- Location: `/home/mike/HeadUnit/external/porcupine/`
- Currently working with "jarvis" wake word

### 2. Picovoice Access Key
Already configured in main.cpp:73
```cpp
wakeWordDetector.setAccessKey("YOUR_PICOVOICE_ACCESS_KEY");
```

## Architecture Overview

### Current Architecture (Has Issues)
```
Microphone → Porcupine (wake word)
          ↓ (wake word detected)
          → Microphone switch issue ← Google STT
          ↓
          → Claude AI
          ↓
          → Google TTS → Speaker
```

### Target Architecture (Unified Pipeline)
```
Microphone (16kHz, mono, int16)
    ↓
Koala (noise suppression)
    ↓
Porcupine (wake word detection "jarvis")
    ↓ (wake word detected)
    ↓
Rhino (intent detection) → Fast path for simple commands
    ├── MakeCall intent → VoiceCommandHandler
    ├── SendMessage intent → VoiceCommandHandler
    ├── PlayMusic intent → MediaController
    ├── Navigate intent → Navigation
    ├── ControlMusic intent → MediaController
    └── No intent match → Fall through
         ↓
    Leopard (full STT) → Complex queries
         ↓
    Claude AI
    ↓
Google TTS → Speaker
```

## Remaining Implementation Tasks

### Task 1: Create Rhino Context File
**Priority:** HIGH (Required before Rhino can be used)

Create a custom Rhino context file using Picovoice Console (https://console.picovoice.ai/):

**Intents to Define:**
```yaml
context_name: headunit

intents:
  - MakeCall:
      expressions:
        - "call $contact_name"
        - "phone $contact_name"
        - "dial $contact_name"
        - "ring $contact_name"
      slots:
        - contact_name

  - SendMessage:
      expressions:
        - "text $contact_name"
        - "message $contact_name"
        - "send a message to $contact_name"
        - "send a text to $contact_name"
      slots:
        - contact_name

  - PlayMusic:
      expressions:
        - "play $song_title"
        - "play music by $artist_name"
        - "play $artist_name"
      slots:
        - song_title
        - artist_name

  - Navigate:
      expressions:
        - "navigate to $location"
        - "directions to $location"
        - "take me to $location"
        - "drive to $location"
      slots:
        - location

  - ControlMusic:
      expressions:
        - "$action music"
        - "$action the song"
        - "$action"
      slots:
        - action: [play, pause, stop, next, previous, skip]

  - AdjustVolume:
      expressions:
        - "volume $level"
        - "$level volume"
        - "turn volume $level"
      slots:
        - level: [up, down, max, mute, unmute]
```

**Steps:**
1. Log in to Picovoice Console
2. Create new Rhino context named "headunit"
3. Define the intents above
4. Train and download `headunit_raspberry-pi.rhn` context file
5. Place in `/home/mike/HeadUnit/external/rhino/contexts/`

### Task 2: Create PicovoiceManager Class
**Files to Create:**
- `/home/mike/HeadUnit/PicovoiceManager.h`
- `/home/mike/HeadUnit/PicovoiceManager.cpp`

**Class Structure:**
```cpp
class PicovoiceManager : public QObject {
    Q_OBJECT

public:
    explicit PicovoiceManager(QObject *parent = nullptr);
    ~PicovoiceManager();

    void start();
    void stop();
    void pause();
    void resume();

    // Configuration
    void setAccessKey(const QString &key);
    void setSensitivity(float sensitivity);
    void setWakeWord(const QString &keyword);

signals:
    void wakeWordDetected(const QString &keyword);
    void intentDetected(const QString &intent, const QVariantMap &slots);
    void transcriptionReady(const QString &text);
    void error(const QString &message);
    void statusChanged(const QString &status);

private slots:
    void onAudioReady();

private:
    // Picovoice components
    pv_porcupine_t *m_porcupine;
    pv_rhino_t *m_rhino;
    pv_leopard_t *m_leopard;
    pv_koala_t *m_koala;

    // Audio pipeline
    QAudioSource *m_audioSource;
    QIODevice *m_audioDevice;
    QByteArray m_audioBuffer;

    // State machine
    enum State {
        Listening,          // Listening for wake word
        WaitingForCommand,  // Wake word detected, collecting audio for Rhino
        ProcessingSpeech    // Rhino finished, processing with Leopard
    };
    State m_state;

    // Audio accumulation for Leopard
    QVector<int16_t> m_speechBuffer;

    // Helper methods
    bool initializePorcupine();
    bool initializeRhino();
    bool initializeLeopard();
    bool initializeKoala();

    void processAudioFrame(const int16_t *frame, int32_t length);
    void processWakeWord(const int16_t *frame);
    void processRhinoIntent(const int16_t *frame);
    void processLeopardSpeech();

    void cleanup();
};
```

### Task 3: Implement Continuous Audio Pipeline
**Key Implementation Points:**

1. **Single Continuous Audio Stream**
   - One QAudioSource capturing at 16kHz, mono, int16
   - No microphone switching

2. **Audio Flow Through Pipeline:**
```cpp
void PicovoiceManager::onAudioReady() {
    QByteArray data = m_audioDevice->readAll();

    // 1. Apply noise suppression with Koala
    int16_t *clean_audio = applyKoala(data);

    // 2. Process based on current state
    switch (m_state) {
        case Listening:
            processWakeWord(clean_audio);
            break;

        case WaitingForCommand:
            processRhinoIntent(clean_audio);
            break;

        case ProcessingSpeech:
            // Accumulate audio for Leopard
            m_speechBuffer.append(clean_audio);
            break;
    }
}
```

3. **State Transitions:**
   - `Listening` → `WaitingForCommand` (when wake word detected)
   - `WaitingForCommand` → `Listening` (when Rhino matches intent)
   - `WaitingForCommand` → `ProcessingSpeech` (when Rhino doesn't match, need Leopard)
   - `ProcessingSpeech` → `Listening` (after Leopard transcription complete)

### Task 4: Integrate with Existing VoiceCommandHandler
**Modification to VoiceCommandHandler:**

Add new slots to handle intents from Rhino:
```cpp
// VoiceCommandHandler.h
public slots:
    void handleIntent(const QString &intent, const QVariantMap &slots);

private:
    void handleMakeCallIntent(const QString &contactName);
    void handleSendMessageIntent(const QString &contactName);
    void handlePlayMusicIntent(const QVariantMap &slots);
    void handleNavigateIntent(const QString &location);
    void handleControlMusicIntent(const QString &action);
    void handleAdjustVolumeIntent(const QString &level);
```

**Connect PicovoiceManager to VoiceCommandHandler in main.cpp:**
```cpp
connect(&picovoiceManager, &PicovoiceManager::intentDetected,
        &voiceCommandHandler, &VoiceCommandHandler::handleIntent);
```

### Task 5: Claude AI Integration for Complex Queries
**When Rhino doesn't match an intent:**

```cpp
void PicovoiceManager::processRhinoIntent(const int16_t *frame) {
    bool is_finalized = false;
    pv_rhino_process(m_rhino, frame, &is_finalized);

    if (is_finalized) {
        bool is_understood = false;
        pv_rhino_is_understood(m_rhino, &is_understood);

        if (is_understood) {
            // Get intent and emit
            const char *intent;
            int32_t num_slots;
            const char **slots, **values;
            pv_rhino_get_intent(m_rhino, &intent, &num_slots, &slots, &values);

            QVariantMap slotMap;
            for (int i = 0; i < num_slots; i++) {
                slotMap[slots[i]] = values[i];
            }

            emit intentDetected(intent, slotMap);
            pv_rhino_reset(m_rhino);
            m_state = Listening;
        } else {
            // Not understood by Rhino, use Leopard for full transcription
            m_state = ProcessingSpeech;
            // Continue accumulating audio
        }
    }
}

void PicovoiceManager::finalizeLeopardTranscription() {
    char *transcript = nullptr;
    int32_t num_words = 0;
    pv_word_t *words = nullptr;

    pv_leopard_process(m_leopard,
                       m_speechBuffer.data(),
                       m_speechBuffer.size(),
                       &transcript,
                       &num_words,
                       &words);

    emit transcriptionReady(QString(transcript));

    // Free resources
    pv_leopard_transcript_delete(transcript);
    pv_leopard_words_delete(words);

    m_speechBuffer.clear();
    m_state = Listening;
}
```

### Task 6: Update CMakeLists.txt
Add Picovoice libraries to the build:

```cmake
# Picovoice SDK paths
set(PICOVOICE_DIR "${CMAKE_SOURCE_DIR}/external")

# Include directories
include_directories(
    ${PICOVOICE_DIR}/porcupine/include
    ${PICOVOICE_DIR}/leopard/include
    ${PICOVOICE_DIR}/rhino/include
    ${PICOVOICE_DIR}/koala/include
)

# Link libraries
target_link_libraries(appHeadUnit
    # ... existing libraries ...
    ${PICOVOICE_DIR}/porcupine/lib/libpv_porcupine.so
    ${PICOVOICE_DIR}/leopard/lib/libpv_leopard.so
    ${PICOVOICE_DIR}/rhino/lib/libpv_rhino.so
    ${PICOVOICE_DIR}/koala/lib/libpv_koala.so
)
```

### Task 7: Update main.cpp
Replace WakeWordDetector with PicovoiceManager:

```cpp
// Create PicovoiceManager instead of WakeWordDetector
PicovoiceManager picovoiceManager;

// Set access key
picovoiceManager.setAccessKey("YOUR_PICOVOICE_ACCESS_KEY");

// Connect to VoiceCommandHandler
voiceCommandHandler.setPicovoiceManager(&picovoiceManager);

// Connect signals
QObject::connect(&picovoiceManager, &PicovoiceManager::intentDetected,
                 &voiceCommandHandler, &VoiceCommandHandler::handleIntent);
QObject::connect(&picovoiceManager, &PicovoiceManager::transcriptionReady,
                 &claudeClient, &ClaudeClient::sendMessage);

// Expose to QML
engine.rootContext()->setContextProperty("picovoiceManager", &picovoiceManager);

// Start the pipeline
picovoiceManager.start();
```

### Task 8: Update QML UI (Main.qml)
Update to show different status for quick commands vs AI queries:

```qml
Connections {
    target: picovoiceManager

    function onWakeWordDetected(keyword) {
        console.log("Wake word detected:", keyword)
        claudeOverlay.visible = true
        statusText.text = "Listening..."
    }

    function onIntentDetected(intent, slots) {
        console.log("Intent detected:", intent, JSON.stringify(slots))
        statusText.text = "Processing command: " + intent
        // Auto-hide after 2 seconds
        hideTimer.start()
    }

    function onTranscriptionReady(text) {
        console.log("Transcription:", text)
        statusText.text = "You said: " + text
        // Send to Claude AI
        claudeClient.sendMessage(text)
    }
}
```

### Task 9: Remove Google STT Dependencies
1. Remove `GoogleSpeechRecognizer.h` and `GoogleSpeechRecognizer.cpp`
2. Remove Google STT from CMakeLists.txt
3. Remove from main.cpp instantiation
4. Update any remaining references in VoiceAssistant.cpp

### Task 10: Testing & Validation
**Test Cases:**

1. **Wake Word Detection:**
   - Say "Jarvis" → Should trigger system

2. **Quick Commands (Rhino):**
   - "Call John" → Should trigger phone call
   - "Text Sarah" → Should open messaging
   - "Play music by The Beatles" → Should play music
   - "Navigate to home" → Should start navigation
   - "Pause music" → Should pause playback
   - "Volume up" → Should increase volume

3. **Complex Queries (Leopard → Claude):**
   - "What's the weather like today?" → Should use Claude AI
   - "Tell me a joke" → Should use Claude AI
   - "What restaurants are nearby?" → Should use Claude AI

4. **Noise Suppression (Koala):**
   - Test with engine noise
   - Test with wind noise (open window)
   - Test with background conversation

## Benefits of This Architecture

1. **No Microphone Switching** - Single continuous audio stream eliminates the current bug
2. **Faster Response** - Rhino processes intents in <100ms locally
3. **Offline Operation** - Works without cellular coverage
4. **Better Noise Handling** - Koala optimized for automotive environments
5. **Lower Latency** - No cloud round-trip for common commands
6. **Privacy** - Voice processing happens on-device
7. **Natural Language** - Rhino handles command variations

## Resource Files Needed

Before running, ensure these files exist:
- `/home/mike/HeadUnit/external/porcupine/lib/common/porcupine_params.pv`
- `/home/mike/HeadUnit/external/porcupine/resources/keyword_files/raspberry-pi/jarvis_raspberry-pi.ppn`
- `/home/mike/HeadUnit/external/leopard/common/leopard_params.pv`
- `/home/mike/HeadUnit/external/rhino/common/rhino_params.pv`
- `/home/mike/HeadUnit/external/rhino/contexts/headunit_raspberry-pi.rhn` ← **NEEDS TO BE CREATED**

## Estimated Implementation Time

- Task 1 (Rhino Context): 30 minutes
- Task 2 (PicovoiceManager Class): 2-3 hours
- Task 3 (Audio Pipeline): 1-2 hours
- Task 4 (VoiceCommandHandler Integration): 1 hour
- Task 5 (Claude AI Integration): 30 minutes
- Task 6-8 (Build System & UI Updates): 1 hour
- Task 9 (Cleanup): 30 minutes
- Task 10 (Testing): 1-2 hours

**Total: 7-10 hours of development**

## Next Steps

1. Create Rhino context file using Picovoice Console
2. Implement PicovoiceManager class
3. Test incrementally as each component is added
4. Validate complete workflow end-to-end
