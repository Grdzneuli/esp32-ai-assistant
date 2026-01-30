# ESP32-S3 AI Assistant

AI-powered voice assistant for the **ESP32-S3 AI Board** using Google Gemini API.

## Features

- **Voice Interaction** - I2S microphone input with voice activity detection
- **Audio Output** - I2S amplifier for feedback sounds and TTS
- **1.9" TFT Display** - Chat UI with status indicators
- **Web Interface** - Browser-based chat over WiFi
- **Button Controls** - BOOT (talk), VOL+, VOL-

## Hardware

**ESP32-S3 AI Board** with integrated:

| Component | Pins |
|-----------|------|
| LCD (ST7789 170x320) | DC=11, SDA=10, SCL=12, CS=13, RST=14, BL=3 |
| I2S Microphone | SCK=5, SD=6, WS=4 |
| I2S Amplifier | DIN=7, BCLK=15, LRCLK=16 |
| Volume Buttons | VOL+=40, VOL-=39 |
| BOOT Button | GPIO 0 |
| WS2812 LED | GPIO 48 |

## Setup

1. **Install PlatformIO** in VS Code

2. **Configure WiFi & API Key** - Edit `src/config.h`:
   ```cpp
   #define WIFI_SSID     "your-wifi"
   #define WIFI_PASSWORD "your-password"
   #define GEMINI_API_KEY "your-gemini-api-key"
   ```

3. **Get Gemini API Key** from [Google AI Studio](https://aistudio.google.com/apikey)

4. **Build & Upload**:
   ```bash
   pio run -t upload
   pio device monitor
   ```

## Usage

- **BOOT button** - Press to start/stop voice input
- **VOL+/VOL-** - Adjust speaker volume
- **Web UI** - Open `http://<device-ip>` in browser

## Project Structure

```
src/
├── main.cpp          # Main logic & state machine
├── config.h          # Pin & API configuration
├── wifi_manager.*    # WiFi connection
├── gemini_client.*   # Gemini API client
├── display.*         # TFT display UI
├── audio_input.*     # I2S microphone
├── audio_output.*    # I2S speaker
├── buttons.*         # Button handling
├── led.*             # WS2812 status LED
└── web_server.*      # Web interface
```

## Notes

- Voice-to-text requires Google Cloud Speech API integration (placeholder in code)
- Text-to-speech requires Google Cloud TTS API integration (placeholder in code)
- The current implementation uses text input via web interface for full functionality

## License

MIT
