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
)

# TextToSpeech is optional - only use if available
find_package(Qt6 COMPONENTS TextToSpeech QUIET)

# Qt 6.2 compatible setup (no REQUIRES parameter)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

qt_add_executable(appHeadUnit
    main.cpp
    tidalcontroller.cpp
    MediaController.cpp
    VoiceAssistant.cpp
    NotificationManager.cpp
)

target_sources(appHeadUnit PRIVATE
    tidalcontroller.h
    MediaController.h
    VoiceAssistant.h
    NotificationManager.h
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
        Ui/screens/Home.qml
        Ui/screens/Music.qml
        Ui/screens/Maps.qml
        Ui/screens/Settings.qml
        Ui/screens/Tidal.qml
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
        Ui/TidalWeb.qml
        Ui/TidalNative.qml
        Ui/MapWeb.qml
    RESOURCES
        assets/html/tidal_player.html
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
        themes/Cyberpunk/icons/tidal_cyber.svg
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
        themes/RetroVFD/icons/tidal_vfd.svg
        themes/RetroVFD/icons/spotify_vfd.svg
        themes/RetroVFD/icons/radio_vfd.svg
        themes/RetroVFD/icons/podcasts_vfd.svg
        themes/RetroVFD/icons/climate_vfd.svg
        themes/RetroVFD/icons/vehicle_vfd.svg
        themes/RetroVFD/icons/camera_vfd.svg
        themes/RetroVFD/icons/weather_vfd.svg
)

target_link_libraries(appHeadUnit PRIVATE
    Qt6::Quick
    Qt6::Svg
    Qt6::WebEngineQuick
    Qt6::WebChannel
    Qt6::Bluetooth
    Qt6::DBus
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
