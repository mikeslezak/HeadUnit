# Sammy AI Voice Assistant Integration Plan

## Project Overview
Integrate Claude AI as "Sammy" - an intelligent voice assistant for the HeadUnit system with wake word detection, natural language processing, system monitoring, and phone integration.

---

## Phase 1: Foundation & Wake Word Detection

### 1.1 Wake Word Detection ("Hey Sammy")
- [ ] **CHOSEN:** Porcupine (Picovoice) - best balance of accuracy and performance
  - Lightweight (runs on embedded devices)
  - Works completely offline
  - Free tier available for development
  - Can train custom "Hey Sammy" wake word
- [ ] Install Porcupine SDK for Linux/ARM
- [ ] Create C++ wrapper for wake word detection
- [ ] Integrate with existing VoiceAssistant class
- [ ] Test wake word accuracy and latency
- [ ] Add configuration for sensitivity settings

### 1.2 Audio Pipeline Setup
- [ ] Review existing audio capture setup
- [ ] Ensure continuous microphone monitoring
- [ ] Implement audio preprocessing (noise reduction, VAD)
- [ ] Set up audio buffering for wake word detection
- [ ] Test audio quality and responsiveness

---

## Phase 2: Claude AI Integration

### 2.1 Claude API Setup
- [ ] Obtain Anthropic API key
- [ ] Create secure storage for API credentials
- [ ] Set up API client in C++ (using libcurl or Qt Network)
- [ ] Implement request/response handling
- [ ] Add error handling and retry logic
- [ ] Implement streaming responses for faster interaction

### 2.2 Speech-to-Text (STT)
- [ ] **PRIMARY:** Google Cloud Speech-to-Text (cloud, high accuracy)
  - Excellent automotive noise handling
  - Real-time streaming support
  - ~$0.006 per 15 seconds
- [ ] **FALLBACK:** Vosk (offline, free)
  - For basic commands when offline
  - Acceptable accuracy for simple tasks
- [ ] Implement STT integration with auto-fallback
- [ ] Optimize for automotive environment (noise, accents)
- [ ] Test transcription accuracy
- [ ] Add language/accent configuration

### 2.3 Text-to-Speech (TTS)
- [ ] **PRIMARY:** Google Cloud TTS with WaveNet/Neural2 voices (cloud)
  - Natural female voice selection
  - Adjustable speaking rate and pitch
  - ~$16 per 1 million characters
- [ ] **FALLBACK:** eSpeak-ng or Piper TTS (offline)
  - Lower quality but functional
  - For offline scenarios
- [ ] Configure "Sammy" personality: warm, helpful, conversational female voice
- [ ] Test speech quality and naturalness

---

## Phase 3: Context & System Integration

### 3.1 System Context Provider
- [ ] Create SystemContextManager class
- [ ] Implement current context gathering:
  - [ ] Current screen/app
  - [ ] Music playback state
  - [ ] Navigation status
  - [ ] Bluetooth connection status
  - [ ] Time, date, location
  - [ ] Recent notifications
  - [ ] Call history
  - [ ] Contact list access
- [ ] Format context for Claude prompts
- [ ] Implement context caching for efficiency

### 3.2 System Monitoring
- [ ] Create SystemMonitor class
- [ ] Monitor system resources:
  - [ ] CPU usage
  - [ ] Memory usage
  - [ ] Disk space
  - [ ] Network connectivity
  - [ ] Bluetooth status
  - [ ] GPS signal strength
- [ ] Implement proactive monitoring alerts
- [ ] Create diagnostic reporting for Claude

---

## Phase 4: Action Execution

### 4.1 Function Calling Framework
- [ ] Design action execution architecture
- [ ] Implement available actions:
  - [ ] **Navigation**: Set destination, get directions
  - [ ] **Phone**: Call contact, answer call, hang up, send SMS
  - [ ] **Media**: Play/pause, next/previous track, volume control
  - [ ] **Settings**: Adjust brightness, toggle features
  - [ ] **Information**: Weather, time, calendar
  - [ ] **System**: Open apps, navigate screens
- [ ] Create action validators (safety checks)
- [ ] Implement confirmation flow for critical actions
- [ ] Add action logging

### 4.2 Contact & Phone Integration
- [ ] Access existing ContactManager
- [ ] Implement contact search/matching:
  - [ ] Fuzzy name matching
  - [ ] Nickname support
  - [ ] Disambiguation ("Which John?")
- [ ] Integrate with phone calling functionality
- [ ] Implement SMS sending via Bluetooth
- [ ] Add call history context

---

## Phase 5: Conversation & Memory

### 5.1 Conversation Management
- [ ] Implement conversation history storage
- [ ] Create session management:
  - [ ] Track multi-turn conversations
  - [ ] Maintain context across queries
  - [ ] Handle interruptions
- [ ] Implement conversation summarization
- [ ] Add conversation reset/clear functionality

### 5.2 Persistent Memory
- [ ] Design memory storage (SQLite or JSON)
- [ ] Store user preferences:
  - [ ] Favorite contacts
  - [ ] Common destinations
  - [ ] Music preferences
  - [ ] Routine patterns
- [ ] Implement memory retrieval for Claude context
- [ ] Add privacy controls for memory

---

## Phase 6: UI/UX Integration

### 6.1 Visual Feedback
- [ ] Enhance VoiceControl.qml for Sammy:
  - [ ] Wake word detection indicator
  - [ ] Listening animation
  - [ ] Thinking/processing state
  - [ ] Speaking indicator
  - [ ] Transcription display
- [ ] Add ambient light ring or visual cue
- [ ] Implement error state displays

### 6.2 Settings Interface
- [ ] Create Sammy configuration screen:
  - [ ] Wake word sensitivity
  - [ ] Voice selection
  - [ ] Privacy settings
  - [ ] API usage monitoring
  - [ ] Conversation history management
- [ ] Add quick toggle for Sammy on/off

---

## Phase 7: Safety & Optimization

### 7.1 Driver Safety
- [ ] Implement driving mode detection
- [ ] Add safety confirmations for critical actions
- [ ] Limit complex interactions while driving
- [ ] Implement auto-pause when driver attention needed
- [ ] Add "do not disturb" mode

### 7.2 Performance Optimization
- [ ] Optimize API call frequency
- [ ] Implement response caching
- [ ] Add local fallbacks for common queries
- [ ] Optimize audio processing pipeline
- [ ] Monitor and limit data usage

### 7.3 Privacy & Security
- [ ] Implement secure credential storage
- [ ] Add conversation encryption (optional)
- [ ] Create privacy mode (disable recording)
- [ ] Implement data retention policies
- [ ] Add audit logging

---

## Phase 8: Testing & Refinement

### 8.1 Testing
- [ ] Unit tests for all components
- [ ] Integration tests for conversation flows
- [ ] Real-world driving scenario tests
- [ ] Noise and accent variation testing
- [ ] Performance benchmarking

### 8.2 User Experience
- [ ] Collect usage patterns
- [ ] Refine prompts based on interactions
- [ ] Optimize response times
- [ ] Improve error handling
- [ ] Add helpful suggestions

---

## Technical Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     User Interface                       │
│                  (VoiceControl.qml)                     │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│              Sammy AI Coordinator                       │
│         (SammyAssistant C++ Class)                      │
└─┬───────────┬──────────┬──────────┬──────────┬─────────┘
  │           │          │          │          │
  ▼           ▼          ▼          ▼          ▼
┌───────┐ ┌──────┐ ┌─────────┐ ┌────────┐ ┌──────────┐
│Wake   │ │ STT  │ │ Claude  │ │ TTS    │ │ Action   │
│Word   │ │Engine│ │   API   │ │Engine  │ │Executor  │
└───────┘ └──────┘ └─────────┘ └────────┘ └──────────┘
                         │
                         ▼
              ┌──────────────────────┐
              │  System Context      │
              │  - Screen State      │
              │  - Media State       │
              │  - Phone State       │
              │  - Navigation        │
              │  - Contacts          │
              └──────────────────────┘
```

---

## Current Status: Planning Phase
**Last Updated:** 2025-01-21

### Completed:
- ✅ Project plan created
- ✅ Architecture designed
- ✅ User preferences collected
- ✅ Technology stack selected

### Technology Stack Decided:
- **Wake Word:** Porcupine (Picovoice)
- **STT Primary:** Google Cloud Speech-to-Text
- **STT Fallback:** Vosk (offline)
- **TTS Primary:** Google Cloud TTS (WaveNet female voice)
- **TTS Fallback:** Piper TTS (offline)
- **AI Brain:** Claude API (Anthropic)

### In Progress:
- ⏳ Ready to start Phase 1.1: Wake Word Detection

### Next Steps:
1. ✅ User to obtain Anthropic API key
2. Install Porcupine SDK
3. Create WakeWordDetector C++ class
4. Integrate with existing VoiceAssistant
5. Test "Hey Sammy" detection accuracy

---

## Notes & Decisions

### Design Decisions:
- **Wake Word:** "Hey Sammy" chosen for personal, friendly tone
- **Offline Priority:** Prefer offline solutions where possible for privacy and reliability
- **Safety First:** All driving-related safety checks are non-negotiable

### User Preferences (Decided):
1. ✅ **API Setup:** Need to obtain Anthropic API key (est. $10-30/month usage)
2. ✅ **Connectivity:** Hybrid approach - primarily online, offline fallback for basic commands
3. ✅ **Voice:** Female voice, warm and friendly personality
4. ✅ **Safety:** Always confirm actions initially, reduce confirmations after accuracy validated
5. ✅ **Privacy:** Cloud-based primary (better quality), local fallback when offline

---

## Resources & Links

- Anthropic Claude API: https://docs.anthropic.com/
- Porcupine Wake Word: https://picovoice.ai/platform/porcupine/
- Vosk STT: https://alphacephei.com/vosk/
- Qt Speech API: https://doc.qt.io/qt-6/qtspeech-index.html

