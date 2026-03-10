cmake_minimum_required(VERSION 3.21)

project(HeadUnit VERSION 1.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(APP_VERSION="${PROJECT_VERSION}")

# Qt 6.2 compatible (Jetson Orin Nano)
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Quick
    Svg
    Bluetooth
    DBus
    Multimedia
    Network
    Location
    Positioning
)

# TextToSpeech is optional - only use if available
find_package(Qt6 COMPONENTS TextToSpeech QUIET)

# MapLibre Native Qt (native map rendering, replaces WebEngine)
set(MAPLIBRE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/maplibre-install")
find_package(QMapLibre COMPONENTS Core Location REQUIRED
    PATHS "${MAPLIBRE_DIR}/lib/cmake/QMapLibre" NO_DEFAULT_PATH
)

# Qt 6.2 compatible setup (no REQUIRES parameter)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_add_executable(appHeadUnit
    main.cpp
    MediaController.cpp
    VoiceAssistant.cpp
    PicovoiceManager.cpp
    ClaudeClient.cpp
    GoogleTTS.cpp
    GoogleSTT.cpp
    NotificationManager.cpp
    BluetoothManager.cpp
    BluetoothDeviceModel.cpp
    TelephonyManager.cpp
    ContactManager.cpp
    MessageManager.cpp
    VoiceCommandHandler.cpp
    WeatherManager.cpp
    VehicleBusManager.cpp
    TidalClient.cpp
    SpotifyClient.cpp
    UpdateManager.cpp
)

target_sources(appHeadUnit PRIVATE
    MediaController.h
    VoiceAssistant.h
    PicovoiceManager.h
    ClaudeClient.h
    GoogleTTS.h
    GoogleSTT.h
    NotificationManager.h
    BluetoothManager.h
    BluetoothDeviceModel.h
    TelephonyManager.h
    ContactManager.h
    MessageManager.h
    VoiceCommandHandler.h
    WeatherManager.h
    VehicleBusManager.h
    TidalClient.h
    SpotifyClient.h
    UpdateManager.h
)

set_source_files_properties(ThemeValues.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appHeadUnit
    URI HeadUnit
    VERSION 1.0
    QML_FILES
        Main.qml
        Theme.qml
        ThemeValues.qml
        FontLoader.qml
        Ui/SplashScreen.qml
        Ui/Page.qml
        Ui/Button.qml
        Ui/GlowButton.qml
        Ui/GlowActionIcon.qml
        Ui/NavBar.qml
        Ui/StatusBar.qml
        Ui/NotificationPanel.qml
        Ui/NotificationBanner.qml
        Ui/VoiceControl.qml
        Ui/VoicePipeline.qml
        Ui/ScreenContainer.qml
        Ui/ClaudeIndicator.qml
        Ui/CallOverlayBase.qml
        Ui/ActiveCallOverlay.qml
        Ui/IncomingCallOverlay.qml
        Ui/MiniPlayer.qml
        Ui/BluetoothHandler.qml
        Ui/AppGridOverlay.qml
        Ui/screens/Home.qml
        Ui/screens/Music.qml
        Ui/screens/Maps.qml
        Ui/screens/Settings.qml
        Ui/settings/DisplaySettings.qml
        Ui/settings/BluetoothSettings.qml
        Ui/settings/VoiceSettings.qml
        Ui/settings/TimeSettings.qml
        Ui/settings/AboutSettings.qml
        Ui/settings/AdvancedSettings.qml
        Ui/screens/Phone.qml
        Ui/screens/Messages.qml
        Ui/screens/Contacts.qml
        Ui/screens/Weather.qml
        Ui/screens/Vehicle.qml
        Ui/screens/Tuning.qml
        Ui/screens/Tidal.qml
        Ui/screens/Spotify.qml
    RESOURCES
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
        themes/Cyberpunk/icons/weather_cyber.svg
        themes/Cyberpunk/icons/vehicle_cyber.svg
        themes/Cyberpunk/icons/tuning_cyber.svg
        themes/Cyberpunk/icons/tidal_cyber.svg
        themes/Cyberpunk/icons/spotify_cyber.svg
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
        themes/RetroVFD/icons/weather_vfd.svg
        themes/RetroVFD/icons/vehicle_vfd.svg
        themes/RetroVFD/icons/tuning_vfd.svg
        themes/RetroVFD/icons/tidal_vfd.svg
        themes/RetroVFD/icons/spotify_vfd.svg
        themes/Monochrome/tokens.json
        themes/Monochrome/fonts/InterDisplay-Regular.ttf
        themes/Monochrome/logo.svg
        themes/Monochrome/icons/home_mono.svg
        themes/Monochrome/icons/music_mono.svg
        themes/Monochrome/icons/maps_mono.svg
        themes/Monochrome/icons/settings_mono.svg
        themes/Monochrome/icons/play_mono.svg
        themes/Monochrome/icons/pause_mono.svg
        themes/Monochrome/icons/next_mono.svg
        themes/Monochrome/icons/previous_mono.svg
        themes/Monochrome/icons/search_mono.svg
        themes/Monochrome/icons/phone_mono.svg
        themes/Monochrome/icons/messages_mono.svg
        themes/Monochrome/icons/contacts_mono.svg
        themes/Monochrome/icons/weather_mono.svg
        themes/Monochrome/icons/vehicle_mono.svg
        themes/Monochrome/icons/tuning_mono.svg
        themes/Monochrome/icons/tidal_mono.svg
        themes/Monochrome/icons/spotify_mono.svg
)

# GStreamer (for Tidal audio playback via playbin)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)

# Picovoice SDK paths
set(PICOVOICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(PORCUPINE_DIR "${PICOVOICE_DIR}/porcupine")
set(LEOPARD_DIR "${PICOVOICE_DIR}/leopard")
set(RHINO_DIR "${PICOVOICE_DIR}/rhino")
set(KOALA_DIR "${PICOVOICE_DIR}/koala")

# Include directories
target_include_directories(appHeadUnit PRIVATE
    "${PORCUPINE_DIR}/include"
    "${LEOPARD_DIR}/include"
    "${RHINO_DIR}/include"
    "${KOALA_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    ${GST_INCLUDE_DIRS}
)

# Verify Picovoice libraries exist before linking
set(PV_PORCUPINE_LIB "${PORCUPINE_DIR}/lib/raspberry-pi/cortex-a72-aarch64/libpv_porcupine.so")
set(PV_LEOPARD_LIB "${LEOPARD_DIR}/lib/libpv_leopard.so")
set(PV_RHINO_LIB "${RHINO_DIR}/lib/libpv_rhino.so")
set(PV_KOALA_LIB "${KOALA_DIR}/lib/libpv_koala.so")

foreach(LIB ${PV_PORCUPINE_LIB} ${PV_LEOPARD_LIB} ${PV_RHINO_LIB} ${PV_KOALA_LIB})
    if(NOT EXISTS "${LIB}")
        message(WARNING "Picovoice library not found: ${LIB}")
    endif()
endforeach()

# Link Picovoice libraries (cortex-a72 for Jetson Tegra, compatible with Cortex-A78AE)
target_link_libraries(appHeadUnit PRIVATE
    "${PV_PORCUPINE_LIB}"
    "${PV_LEOPARD_LIB}"
    "${PV_RHINO_LIB}"
    "${PV_KOALA_LIB}"
)

target_link_libraries(appHeadUnit PRIVATE
    Qt6::Quick
    Qt6::Svg
    Qt6::Bluetooth
    Qt6::DBus
    Qt6::Multimedia
    Qt6::Network
    Qt6::Location
    Qt6::Positioning
    QMapLibre::Core
    QMapLibre::Location
    ${GST_LIBRARIES}
)

qmaplibre_location_setup_plugins(appHeadUnit)

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
