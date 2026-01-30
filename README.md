# ESP32-S3 AI Assistant

A fully voice-enabled AI assistant for the **ESP32-S3**, powered by Google Gemini, Cloud Speech-to-Text, and Cloud Text-to-Speech APIs.

## Features

- **Full Voice Interaction** - Speak naturally, hear AI responses
- **Speech-to-Text** - Google Cloud Speech API transcription
- **Text-to-Speech** - Neural voice synthesis for natural responses
- **AI Powered** - Google Gemini for intelligent conversations
- **1.9" TFT Display** - Real-time chat UI with message history
- **Status LED** - WS2812 RGB LED with state animations
- **Voice Activity Detection** - Automatic silence detection
- **Interruptible** - Stop responses mid-playback

## How It Works

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Record    │───▶│   Google    │───▶│   Google    │───▶│   Google    │
│   Audio     │    │  STT API    │    │  Gemini AI  │    │  TTS API    │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
      │                  │                  │                  │
      ▼                  ▼                  ▼                  ▼
  Microphone         Transcribe          Generate           Synthesize
   captures           speech              AI reply            speech
    voice            to text                                 audio
```

## Hardware Requirements

| Component | Specification |
|-----------|--------------|
| MCU | ESP32-S3 (16MB Flash, PSRAM recommended) |
| Display | 1.9" IPS TFT ST7789 (170x320) |
| Microphone | I2S Digital MEMS |
| Speaker | I2S Audio Amplifier |
| LED | WS2812 RGB |
| Buttons | BOOT + VOL+/- |

### Pin Configuration

| Component | Pins |
|-----------|------|
| LCD (ST7789) | DC=11, MOSI=10, SCLK=12, CS=13, RST=14, BL=3 |
| I2S Microphone | SCK=5, SD=6, WS=4 |
| I2S Amplifier | DIN=7, BCLK=15, LRCLK=16 |
| Buttons | BOOT=0, VOL+=40, VOL-=39 |
| WS2812 LED | GPIO 48 |

## Quick Start

### 1. Clone & Configure

```bash
git clone https://github.com/YOUR_USERNAME/esp32-ai-assistant.git
cd esp32-ai-assistant
cp src/config.h.example src/config.h
```

### 2. Get API Keys

You need **two** API keys:

| API | Purpose | Get it from |
|-----|---------|-------------|
| Gemini API | AI responses | [Google AI Studio](https://aistudio.google.com/apikey) |
| Google Cloud API | STT & TTS | [Google Cloud Console](https://console.cloud.google.com/apis/credentials) |

**Enable these Google Cloud APIs:**
- Cloud Speech-to-Text API
- Cloud Text-to-Speech API

### 3. Edit Configuration

Edit `src/config.h`:

```cpp
// WiFi
#define WIFI_SSID          "your-wifi-ssid"
#define WIFI_PASSWORD      "your-wifi-password"

// Gemini AI
#define GEMINI_API_KEY     "your-gemini-api-key"

// Google Cloud (STT/TTS)
#define GOOGLE_CLOUD_API_KEY  "your-google-cloud-api-key"

// Optional: Change language/voice
#define SPEECH_LANGUAGE    "en-US"
#define TTS_VOICE          "en-US-Neural2-A"
```

### 4. Build & Upload

```bash
pio run -t upload
pio device monitor
```

## Usage

### Button Controls

| Button | State | Action |
|--------|-------|--------|
| **BOOT** | Idle | Start recording |
| **BOOT** | Listening | Stop recording & process |
| **BOOT** | Responding | Interrupt playback |
| **VOL+** | Any | Increase volume (+10%) |
| **VOL-** | Any | Decrease volume (-10%) |

### LED Status Guide

| Color | Animation | State |
|-------|-----------|-------|
| Cyan | Breathing | Connecting to WiFi |
| Dim White | Solid | Idle (ready) |
| Red | Pulsing | Listening (recording) |
| Cyan | Breathing | Processing (thinking) |
| Yellow | Pulsing | Speaking (TTS playback) |
| Red | Solid | Error |

### Voice Interaction Flow

1. **Press BOOT** - Hear start sound, LED turns red
2. **Speak** - Talk naturally to the device
3. **Release or wait** - Silence detection auto-stops recording
4. **Processing** - LED turns cyan, "Thinking..." on display
5. **Response** - AI response shown on screen & spoken aloud
6. **Interrupt** - Press BOOT anytime to stop playback

## Project Structure

```
esp32-ai-assistant/
├── src/
│   ├── main.cpp           # Application entry & state machine
│   ├── config.h           # Configuration (WiFi, API keys, pins)
│   ├── speech_client.*    # Google Cloud STT/TTS client
│   ├── gemini_client.*    # Google Gemini AI client
│   ├── wifi_manager.*     # WiFi connection handling
│   ├── display.*          # TFT display UI
│   ├── audio_input.*      # I2S microphone capture
│   ├── audio_output.*     # I2S speaker playback
│   ├── buttons.*          # Button input handling
│   ├── led.*              # WS2812 status LED
│   └── web_server.*       # Optional web interface
├── docs/
│   └── user-manual.html   # Interactive user manual
├── platformio.ini         # Build configuration
└── README.md
```

## Configuration Options

### Language & Voice

```cpp
// Speech recognition language
#define SPEECH_LANGUAGE    "en-US"   // or "es-ES", "de-DE", "fr-FR", etc.

// TTS voice (see: https://cloud.google.com/text-to-speech/docs/voices)
#define TTS_VOICE          "en-US-Neural2-A"  // Female
#define TTS_VOICE          "en-US-Neural2-D"  // Male
```

### Audio Settings

```cpp
#define DEFAULT_VOLUME     70        // 0-100
#define VAD_THRESHOLD      500       // Voice detection sensitivity
#define VAD_SILENCE_MS     1500      // Silence duration to stop recording
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No WiFi connection | Check SSID/password, ensure 2.4GHz network |
| STT not working | Verify Google Cloud API key, check Speech API enabled |
| TTS silent | Check volume level, verify TTS API enabled |
| Audio too quiet | Increase `DEFAULT_VOLUME` or use VOL+ button |
| Recording too short | Lower `VAD_THRESHOLD` or increase `VAD_SILENCE_MS` |

## Dependencies

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) - Display driver
- [ArduinoJson](https://arduinojson.org/) - JSON parsing
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) - Async networking
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Web server
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) - LED control

## Memory Usage

```
RAM:   14.3% (46,732 / 327,680 bytes)
Flash: 15.1% (992,081 / 6,553,600 bytes)
```

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- Powered by [Google Gemini](https://ai.google.dev/)
- Speech services by [Google Cloud](https://cloud.google.com/speech-to-text)
- Built with [PlatformIO](https://platformio.org/)
