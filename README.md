# APEX Firmware

Advanced Penetration & EXploitation firmware for ESP32 devices.

## Features
- Wi-Fi Attacks & Deauthentication
- Bluetooth Exploits & Spam
- RFID & NFC Cloning
- Infrared Remote Emulation
- Sub-GHz Signal Reading & Emulation
- Evil Portal Phishing
- Packet Sniffing & Analysis

## Installation
1. Download the latest release for your device.
2. Flash using WebFlasher or PlatformIO.

## Development
Lead Developer: 3xecutablefile

### Building from Source
1. Install [PlatformIO](https://platformio.org/).
2. Clone this repository: `git clone <repo-url>`
3. Navigate to the project directory: `cd apex-firmware`
4. Build for your device: `pio run -e <board-name>` (see platformio.ini for available boards)
5. (Optional) Merge binaries if needed: `python merge_bins.py`

### Flashing
1. Connect your ESP32 device via USB.
2. Flash the firmware: `pio run -e <board-name> -t upload`

## License
AGPL-3.0