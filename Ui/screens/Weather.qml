import QtQuick 2.15
import HeadUnit

Item {
    id: root
    property var theme: null

    Rectangle {
        anchors.fill: parent
        color: ThemeValues.bgCol

        // Loading state
        Text {
            anchors.centerIn: parent
            visible: weatherManager.loading && weatherManager.temperature === 0
            text: "Loading weather..."
            color: ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize + 4; font.family: ThemeValues.fontFamily
            opacity: 0.6

            SequentialAnimation on opacity {
                running: weatherManager.loading
                loops: Animation.Infinite
                NumberAnimation { from: 0.6; to: 0.2; duration: 800 }
                NumberAnimation { from: 0.2; to: 0.6; duration: 800 }
            }
        }

        // Main content
        Item {
            anchors.fill: parent
            anchors.margins: 20
            visible: !weatherManager.loading || weatherManager.temperature !== 0

            // Left side: Current conditions
            Column {
                id: currentWeather
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * 0.38
                spacing: 12

                // Location & refresh
                Row {
                    width: parent.width
                    spacing: 8

                    Text {
                        text: weatherManager.locationName || "Locating..."
                        color: ThemeValues.primaryCol
                        font.pixelSize: ThemeValues.fontSize + 2; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
                        elide: Text.ElideRight
                        width: parent.width - 80
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 70; height: 30
                        color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                        border.color: ThemeValues.primaryCol; border.width: 1; radius: 6
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: weatherManager.loading ? "..." : "Refresh"
                            color: ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize - 3; font.family: ThemeValues.fontFamily
                        }
                        MouseArea { anchors.fill: parent; onClicked: weatherManager.refresh() }
                    }
                }

                // Big temperature + icon
                Row {
                    spacing: 16
                    anchors.horizontalCenter: parent.horizontalCenter

                    Text {
                        text: weatherManager.weatherIcon
                        font.pixelSize: 72
                        font.family: ThemeValues.fontFamily
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        Text {
                            text: Math.round(weatherManager.temperature) + "°"
                            color: ThemeValues.textCol
                            font.pixelSize: 72; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
                        }

                        Text {
                            text: "C"
                            color: ThemeValues.textCol; opacity: 0.4
                            font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily
                        }
                    }
                }

                Text {
                    text: weatherManager.weatherDescription
                    color: ThemeValues.textCol
                    font.pixelSize: ThemeValues.fontSize + 4; font.family: ThemeValues.fontFamily
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                // Details grid
                Rectangle {
                    width: parent.width; height: 1
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                }

                Grid {
                    columns: 2
                    columnSpacing: 20; rowSpacing: 12
                    anchors.horizontalCenter: parent.horizontalCenter

                    DetailItem { label: "Feels Like"; value: Math.round(weatherManager.feelsLike) + "°C" }
                    DetailItem { label: "Humidity"; value: weatherManager.humidity + "%" }
                    DetailItem { label: "Wind"; value: Math.round(weatherManager.windSpeed) + " km/h" }
                    DetailItem { label: "Direction"; value: windDirectionText(weatherManager.windDirection) }
                }

                // Last updated
                Item { width: 1; height: 1 } // spacer
                Text {
                    text: weatherManager.lastUpdated ? "Updated " + weatherManager.lastUpdated : ""
                    color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize - 3; font.family: ThemeValues.fontFamily; opacity: 0.3
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            // Divider
            Rectangle {
                id: divider
                anchors.left: currentWeather.right
                anchors.leftMargin: 16
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
            }

            // Right side: Forecasts
            Column {
                anchors.left: divider.right
                anchors.leftMargin: 16
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                spacing: 12

                // Hourly forecast header
                Text {
                    text: "Hourly"
                    color: ThemeValues.primaryCol
                    font.pixelSize: ThemeValues.fontSize + 1; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
                }

                // Hourly scroll
                Flickable {
                    width: parent.width
                    height: 100
                    contentWidth: hourlyRow.width
                    clip: true
                    flickableDirection: Flickable.HorizontalFlick

                    Row {
                        id: hourlyRow
                        spacing: 8

                        Repeater {
                            model: weatherManager.hourlyForecast

                            Rectangle {
                                width: 65; height: 90
                                color: index === 0 ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15) : Qt.rgba(ThemeValues.bgCol.r, ThemeValues.bgCol.g, ThemeValues.bgCol.b, 0.3)
                                border.color: index === 0 ? ThemeValues.primaryCol : Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.3)
                                border.width: 1; radius: 8

                                Column {
                                    anchors.centerIn: parent; spacing: 4

                                    Text {
                                        text: index === 0 ? "Now" : modelData.time
                                        color: ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize - 3; font.family: ThemeValues.fontFamily
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }
                                    Text {
                                        text: modelData.icon
                                        font.pixelSize: 24
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }
                                    Text {
                                        text: modelData.temp + "°"
                                        color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
                                        anchors.horizontalCenter: parent.horizontalCenter
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: parent.width; height: 1
                    color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.2)
                }

                // Daily forecast header
                Text {
                    text: "7-Day Forecast"
                    color: ThemeValues.primaryCol
                    font.pixelSize: ThemeValues.fontSize + 1; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
                }

                // Daily forecast list
                Column {
                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: weatherManager.dailyForecast

                        Rectangle {
                            width: parent.width; height: 38
                            color: index === 0 ? Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.1) : "transparent"
                            radius: 6

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 8; anchors.rightMargin: 8
                                spacing: 0

                                Text {
                                    text: modelData.day
                                    color: index === 0 ? ThemeValues.primaryCol : ThemeValues.textCol
                                    font.pixelSize: ThemeValues.fontSize; font.family: ThemeValues.fontFamily
                                    font.weight: index === 0 ? Font.Bold : Font.Normal
                                    width: parent.width * 0.2
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: modelData.icon
                                    font.pixelSize: 20
                                    width: parent.width * 0.15
                                    horizontalAlignment: Text.AlignHCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                // Temperature bar
                                Item {
                                    width: parent.width * 0.45
                                    height: parent.height
                                    anchors.verticalCenter: parent.verticalCenter

                                    Rectangle {
                                        anchors.verticalCenter: parent.verticalCenter
                                        height: 6; radius: 3
                                        x: parent.width * 0.1
                                        width: parent.width * 0.8
                                        color: Qt.rgba(ThemeValues.primaryCol.r, ThemeValues.primaryCol.g, ThemeValues.primaryCol.b, 0.15)

                                        Rectangle {
                                            height: parent.height; radius: 3
                                            color: ThemeValues.primaryCol; opacity: 0.7
                                            x: parent.width * normalizeTemp(modelData.low)
                                            width: parent.width * (normalizeTemp(modelData.high) - normalizeTemp(modelData.low))
                                        }
                                    }
                                }

                                Text {
                                    text: modelData.low + "°"
                                    color: ThemeValues.textCol; opacity: 0.5
                                    font.pixelSize: ThemeValues.fontSize - 1; font.family: ThemeValues.fontFamily
                                    width: parent.width * 0.1
                                    horizontalAlignment: Text.AlignRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: " " + modelData.high + "°"
                                    color: ThemeValues.textCol
                                    font.pixelSize: ThemeValues.fontSize - 1; font.family: ThemeValues.fontFamily; font.weight: Font.Bold
                                    width: parent.width * 0.1
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }
                }
            }
        }

        // Error overlay
        Text {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 10
            visible: weatherManager.errorMessage !== ""
            text: weatherManager.errorMessage
            color: ThemeValues.accentCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily; opacity: 0.7
        }
    }

    // Helper functions
    function windDirectionText(degrees) {
        var dirs = ["N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"]
        return dirs[Math.round(degrees / 22.5) % 16]
    }

    function normalizeTemp(temp) {
        // Normalize temperature to 0-1 range based on daily forecast range
        var forecasts = weatherManager.dailyForecast
        if (forecasts.length === 0) return 0.5
        var minT = 999, maxT = -999
        for (var i = 0; i < forecasts.length; i++) {
            if (forecasts[i].low < minT) minT = forecasts[i].low
            if (forecasts[i].high > maxT) maxT = forecasts[i].high
        }
        var range = maxT - minT
        if (range <= 0) return 0.5
        return (temp - minT) / range
    }

    // Inline components
    component DetailItem: Column {
        spacing: 2
        property string label: ""
        property string value: ""
        Text { text: label; color: ThemeValues.primaryCol; font.pixelSize: ThemeValues.fontSize - 2; font.family: ThemeValues.fontFamily; opacity: 0.7 }
        Text { text: value; color: ThemeValues.textCol; font.pixelSize: ThemeValues.fontSize + 1; font.family: ThemeValues.fontFamily; font.weight: Font.Bold }
    }
}
