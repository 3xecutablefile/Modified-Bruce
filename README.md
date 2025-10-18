# Bruce + Infiltra Hybrid Firmware
Merged pentesting and utility firmware for M5StickC PLUS2

## Features
- Bruce offensive modules: BLE audit, WiFi assessment, web research
- Infiltra utilities: WiFi scanner, IR remote, file tools, device controls

## Installation
1. Install PlatformIO
2. Clone repo
3. Run `pio run -t upload`
4. (Optional) Push bundled IR data with `pio run -t uploadfs`

## Configuration
Edit `src/config.h` to enable/disable modules

## Legal
For authorized testing only. See LICENSE.
