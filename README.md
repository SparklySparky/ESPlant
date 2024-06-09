# ESPlant 🌱

**ESPlant** automates plant watering using ESP32 and lets you monitor everything via an Android app (https://github.com/SparklySparky/ESPlant-Android). Keep your plants happy and hydrated effortlessly!

## Features

- **Automated Watering**: Set watering schedules.
- **Remote Monitoring**: Check status and timers on your Android device.
- **WiFi Connectivity**: Easy setup and control through your home network.
- **Real-Time Updates**: Get real-time data on watering schedules.

## Quick Start

### Hardware Setup

1. **ESP32 Microcontroller**: Connects and controls the water pump.
2. **Water Pump/Valve**: Manages the water flow to your plants.
3. **WiFi Connection**: Connect ESP32 to your home WiFi.
4. **Android Device**: To setup the ESPlant

## Installation

1. **Clone the Repo**:
    ```bash
    git clone https://github.com/SparklySparky/ESPlant-Esp32.git
    ```
2. **Configure WiFi**: Update `CONFIG_ESP_WIFI_SSID` and `CONFIG_ESP_WIFI_PASSWORD` in the code.
3. **Build & Flash**:
    ```bash
    idf.py build
    idf.py flash
    ```
4. **Android App**: follow the instructions here -> https://github.com/SparklySparky/ESPlant-Android.

## Conclusion
Enjoy stress-free plant care with **ESPlant**! For questions or support, open an issue on GitHub. Happy gardening! 🌿
