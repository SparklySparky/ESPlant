# ESPlant 🌱

**ESPlant** automates plant watering using ESP32 and lets you monitor everything via an Android app. Keep your plants happy and hydrated effortlessly!

## Features

- **Automated Watering**: Set and forget watering schedules.
- **Remote Monitoring**: Check status and timers on your Android device.
- **WiFi Connectivity**: Easy setup and control through your home network.
- **Real-Time Updates**: Get real-time data on watering schedules.

## Quick Start

### Hardware Setup

1. **ESP32 Microcontroller**: Connects and controls the water pump.
2. **Water Pump/Valve**: Manages the water flow to your plants.
3. **WiFi Connection**: Connect ESP32 to your home WiFi.

### Android App

- **Monitor & Control**: Check and adjust watering schedules from your phone.
- **Notifications**: Get alerts when watering starts or stops.

## Installation

1. **Clone the Repo**:
    ```bash
    git clone https://github.com/SparklySparky/ESPlant.git
    ```
2. **Configure WiFi**: Update `CONFIG_ESP_WIFI_SSID` and `CONFIG_ESP_WIFI_PASSWORD` in the code.
3. **Build & Flash**:
    ```bash
    idf.py build
    idf.py flash
    ```
4. **Android App**: Install and connect to ESP32.

## Contributing

Fork the repo, make changes, and submit a pull request.

## License

Licensed under the MIT License.

---

Enjoy stress-free plant care with **ESPlant**! For questions or support, open an issue on GitHub. Happy gardening! 🌿
