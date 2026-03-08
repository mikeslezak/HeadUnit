cmake_minimum_required(VERSION 3.21)

project(HeadUnit VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6.2 compatible (Jetson Orin Nano)
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Quick
    Svg
    WebEngineQuick
    WebChannel
    Bluetooth
    DBus
    Multimedia
    Network
)

# TextToSpeech is optional - only use if available
find_package(Qt6 COMPONENTS TextToSpeech QUIET)

# Qt 6.2 compatible setup (no REQUIRES parameter)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_add_executable(appHeadUnit
    main.cpp
    MediaController.cpp
    VoiceAssistant.cpp
    WakeWordDetector.cpp
    PicovoiceManager.cpp
    ClaudeClient.cpp
    GoogleSpeechRecognizer.cpp
    GoogleTTS.cpp
    GoogleSTT.cpp
    NotificationManager.cpp
    BluetoothManager.cpp
    ContactManager.cpp
    MessageManager.cpp
    VoiceCommandHandler.cpp
)

target_sources(appHeadUnit PRIVATE
    MediaController.h
    VoiceAssistant.h
    WakeWordDetector.h
    PicovoiceManager.h
    ClaudeClient.h
    GoogleSpeechRecognizer.h
    GoogleTTS.h
    GoogleSTT.h
    NotificationManager.h
    BluetoothManager.h
    ContactManager.h
    MessageManager.h
    VoiceCommandHandler.h
)

qt_add_qml_module(appHeadUnit
    URI HeadUnit
    VERSION 1.0
    QML_FILES
        Main.qml
        Theme.qml
        FontLoader.qml
        Ui/SplashScreen.qml
        Ui/Page.qml
        Ui/Button.qml
        Ui/GlowButton.qml
        Ui/GlowActionIcon.qml
        Ui/NavBar.qml
        Ui/StatusBar.qml
        Ui/WebTest.qml
        Ui/NotificationPanel.qml
        Ui/NotificationBanner.qml
        Ui/VoiceControl.qml
        Ui/ClaudeIndicator.qml
        Ui/ActiveCallOverlay.qml
        Ui/IncomingCallOverlay.qml
        Ui/MiniPlayer.qml
        Ui/screens/Home.qml
        Ui/screens/Music.qml
        Ui/screens/Maps.qml
        Ui/screens/Settings.qml
        Ui/screens/Spotify.qml
        Ui/screens/Radio.qml
        Ui/screens/Podcasts.qml
        Ui/screens/Climate.qml
        Ui/screens/Vehicle.qml
        Ui/screens/Camera.qml
        Ui/screens/Phone.qml
        Ui/screens/Messages.qml
        Ui/screens/Contacts.qml
        Ui/screens/Weather.qml
        Ui/MapWeb.qml
    RESOURCES
        assets/html/map_view.html
        themes/Cyberpunk/tokens.json
        themes/Cyberpunk/fonts/Orbitron-Regular.ttf
        themes/Cyberpunk/logo.svg
        themes/RetroVFD/tokens.json
        themes/RetroVFD/fonts/DSEG14Classic-Regular.ttf
        themes/RetroVFD/logo.svg
        themes/Cyberpunk/icons/home_cyber.png
        themes/Cyberpunk/icons/music_cyber.png
        themes/Cyberpunk/icons/maps_cyber.png
        themes/Cyberpunk/icons/settings_cyber.png
        themes/Cyberpunk/icons/play_cyber.svg
        themes/Cyberpunk/icons/pause_cyber.svg
        themes/Cyberpunk/icons/next_cyber.svg
        themes/Cyberpunk/icons/previous_cyber.svg
        themes/Cyberpunk/icons/search_cyber.svg
        themes/Cyberpunk/icons/phone_cyber.png
        themes/Cyberpunk/icons/messages_cyber.png
        themes/Cyberpunk/icons/contacts_cyber.png
        themes/Cyberpunk/icons/spotify_cyber.svg
        themes/Cyberpunk/icons/radio_cyber.png
        themes/Cyberpunk/icons/podcast_cyber.png
        themes/Cyberpunk/icons/climate_cyber.png
        themes/Cyberpunk/icons/vehicle_cyber.svg
        themes/Cyberpunk/icons/camera_cyber.png
        themes/Cyberpunk/icons/weather_cyber.svg
        themes/RetroVFD/icons/home_vfd.svg
        themes/RetroVFD/icons/music_vfd.svg
        themes/RetroVFD/icons/maps_vfd.svg
        themes/RetroVFD/icons/settings_vfd.svg
        themes/RetroVFD/icons/play_vfd.svg
        themes/RetroVFD/icons/pause_vfd.svg
        themes/RetroVFD/icons/next_vfd.svg
        themes/RetroVFD/icons/previous_vfd.svg
        themes/RetroVFD/icons/search_vfd.svg
        themes/RetroVFD/icons/phone_vfd.svg
        themes/RetroVFD/icons/messages_vfd.svg
        themes/RetroVFD/icons/contacts_vfd.svg
        themes/RetroVFD/icons/spotify_vfd.svg
        themes/RetroVFD/icons/radio_vfd.svg
        themes/RetroVFD/icons/podcasts_vfd.svg
        themes/RetroVFD/icons/climate_vfd.svg
        themes/RetroVFD/icons/vehicle_vfd.svg
        themes/RetroVFD/icons/camera_vfd.svg
        themes/RetroVFD/icons/weather_vfd.svg
)

# Picovoice SDK paths
set(PICOVOICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(PORCUPINE_DIR "${PICOVOICE_DIR}/porcupine")
set(LEOPARD_DIR "${PICOVOICE_DIR}/leopard")
set(RHINO_DIR "${PICOVOICE_DIR}/rhino")
set(KOALA_DIR "${PICOVOICE_DIR}/koala")

# Include directories for all Picovoice SDKs
target_include_directories(appHeadUnit PRIVATE
    "${PORCUPINE_DIR}/include"
    "${LEOPARD_DIR}/include"
    "${RHINO_DIR}/include"
    "${KOALA_DIR}/include"
)

# Link Picovoice libraries (cortex-a72 for Jetson Tegra, compatible with Cortex-A78AE)
target_link_libraries(appHeadUnit PRIVATE
    "${PORCUPINE_DIR}/lib/raspberry-pi/cortex-a72-aarch64/libpv_porcupine.so"
    "${LEOPARD_DIR}/lib/libpv_leopard.so"
    "${RHINO_DIR}/lib/libpv_rhino.so"
    "${KOALA_DIR}/lib/libpv_koala.so"
)

target_link_libraries(appHeadUnit PRIVATE
    Qt6::Quick
    Qt6::Svg
    Qt6::WebEngineQuick
    Qt6::WebChannel
    Qt6::Bluetooth
    Qt6::DBus
    Qt6::Multimedia
    Qt6::Network
)

# Link TextToSpeech only if available
if(TARGET Qt6::TextToSpeech)
    target_link_libraries(appHeadUnit PRIVATE Qt6::TextToSpeech)
    message(STATUS "TextToSpeech module found - TTS enabled")
else()
    message(WARNING "TextToSpeech module not found - TTS will be disabled (phone voice only)")
endif()

set_target_properties(appHeadUnit PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
