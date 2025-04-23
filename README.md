# Coop Camera
_A battery-powered ESP32-CAM camera accessory for chicken coop monitoring_

## Overview
The Coop Camera is an accessory to the Automated Chicken Coop Ecosystem. It captures still images on demand and delivers them over ESP-NOW via the main chicken door (gateway), where they can be published to MQTT or displayed in the web interface.

It is designed to run on battery power, waking only briefly to take a picture and transmit it, then returning to deep sleep.

---

## Features
- On-demand photo capture via MQTT or Web UI
- Wirelessly transmits images to door via **ESP-NOW**
- Ultra-low power sleep between captures
- Battery-powered with USB charging
- OTA updates over ESP-NOW via the gateway

---

## Hardware
| Component | Purpose |
|----------|---------|
| ESP32 | Dual-core MCU with camera interface support |
| OV2640 | 2MP camera module with JPEG output |
| LiPo Battery + Charger | Power supply for remote use |
| Button | Manual wake and pairing |

---

## Communication
### ESP-NOW
- Camera pairs with the main door
- Captured JPEG images are chunked and streamed over ESP-NOW

### MQTT (via Gateway)
```text
<base_topic>/devices/camera/set/capture        ← true
```

Images can be stored temporarily by the door and fetched via Web UI or API.

---

## Power Management
- Default deep sleep between operations
- Wakes on button press or scheduled request
- Image transmission optimized for speed & minimal power

---

## OTA Updates
- Firmware and filesystem updates delivered via the gateway using ESP-NOW
- Triggered from Web UI or command

---

## Getting Started
1. Flash the firmware:
   ```bash
   cd firmware
   pio run -t upload
   ```
2. Power up the camera and tap the button to start pairing mode.
3. Go to the Web UI (`dvirka.local`), open **Accessories > New > Pair**.
4. Once registered, the camera will be controllable via UI or MQTT.

---

## Author
**Pavel Kejík**  
Part of the Automated Coop Ecosystem – see main project: [chicken-door](https://github.com/pavelkejik/chicken-door)

