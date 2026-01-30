/*
 * ESP32-S3 AI Assistant
 *
 * An AI-powered voice assistant using Google Gemini API
 *
 * Hardware: ESP32-S3 AI Board
 * - 1.9" IPS TFT Display (ST7789 170x320)
 * - I2S Digital Microphone
 * - I2S Audio Amplifier
 * - WS2812 Status LED
 * - Volume +/- Buttons
 * - BOOT button for voice activation
 */

#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "gemini_client.h"
#include "speech_client.h"
#include "display.h"
#include "audio_input.h"
#include "audio_output.h"
#include "buttons.h"
#include "led.h"
#include "wake_word.h"

// Global objects
WiFiManager wifiManager;
GeminiClient gemini;
SpeechClient speech;
Display display;
AudioInput audioInput;
AudioOutput audioOutput;
Buttons buttons;
StatusLED statusLed;
WakeWordDetector wakeWord;

// TTS audio buffer (allocated in PSRAM)
int16_t* ttsBuffer = nullptr;
size_t ttsBufferSize = 0;

// Assistant state machine
enum class AssistantState {
    INIT,
    CONNECTING_WIFI,
    IDLE,
    LISTENING,
    PROCESSING,
    RESPONDING,
    ERROR
};

AssistantState currentState = AssistantState::INIT;
String lastError = "";
int currentVolume = DEFAULT_VOLUME;

// Forward declarations
void setState(AssistantState newState);
void handleButtonEvent(Button button, ButtonEvent event);
void processVoiceInput();
String getTextFromAudio();
void onWakeWordDetected();

// Wake word detection flag (set from callback, processed in main loop)
volatile bool wakeWordTriggered = false;

// Wake word callback - called from detection task
void onWakeWordDetected() {
    wakeWordTriggered = true;
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("   ESP32-S3 AI Assistant Starting...");
    Serial.println("========================================\n");

    // Initialize display first for visual feedback
    display.begin();
    display.showSplash();
    delay(1500);

    // Initialize status LED
    statusLed.begin();
    statusLed.setConnecting();

    // Initialize buttons
    buttons.begin();
    buttons.setCallback(handleButtonEvent);

    // Initialize audio
    if (!audioInput.begin()) {
        Serial.println("[ERROR] Audio input initialization failed");
    }

    if (!audioOutput.begin()) {
        Serial.println("[ERROR] Audio output initialization failed");
    }

    audioOutput.setVolume(currentVolume);

    // Connect to WiFi
    setState(AssistantState::CONNECTING_WIFI);

    wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiManager.setStatusCallback([](WiFiManager::State state, const String& message) {
        switch (state) {
            case WiFiManager::State::CONNECTING:
                display.showStatus("Connecting...", "");
                break;
            case WiFiManager::State::CONNECTED:
                display.showStatus("Connected", wifiManager.getIP());
                break;
            case WiFiManager::State::ERROR:
                display.showError(message);
                break;
            default:
                break;
        }
    });

    if (wifiManager.connect(WIFI_CONNECT_TIMEOUT_MS)) {
        Serial.println("[WiFi] Connected successfully");
        Serial.println("[WiFi] IP: " + wifiManager.getIP());

        // Initialize Gemini client
        gemini.begin(GEMINI_API_KEY);
        gemini.setSystemPrompt(
            "You are a helpful AI assistant running on an ESP32 microcontroller. "
            "Keep your responses concise and friendly, ideally under 100 words. "
            "You can help with general questions, coding, IoT projects, and more."
        );

        // Initialize Speech client (STT/TTS)
        speech.begin(GOOGLE_CLOUD_API_KEY);
        speech.setLanguage(SPEECH_LANGUAGE);
        speech.setVoice(TTS_VOICE);

        // Allocate TTS buffer in PSRAM
        ttsBufferSize = TTS_MAX_SAMPLES;
        if (psramFound()) {
            ttsBuffer = (int16_t*)ps_malloc(ttsBufferSize * sizeof(int16_t));
            Serial.println("[System] TTS buffer allocated in PSRAM");
        } else {
            ttsBuffer = (int16_t*)malloc(ttsBufferSize * sizeof(int16_t));
            Serial.println("[System] TTS buffer allocated in RAM");
        }

        if (!ttsBuffer) {
            Serial.println("[ERROR] Failed to allocate TTS buffer");
        }

        // Initialize wake word detector
        if (WAKE_WORD_ENABLED) {
            if (wakeWord.begin()) {
                wakeWord.setSensitivity(WAKE_WORD_SENSITIVITY);
                wakeWord.setCallback(onWakeWordDetected);
                Serial.println("[System] Wake word detection enabled");
            } else {
                Serial.println("[System] Wake word init failed, button-only mode");
            }
        }

        delay(1000);
        setState(AssistantState::IDLE);
    } else {
        Serial.println("[WiFi] Connection failed");
        setState(AssistantState::ERROR);
        lastError = "WiFi connection failed";
    }

    // Show chat interface
    display.showChat();

    Serial.println("\n[System] Ready! Press BOOT button to talk.");
}

void loop() {
    // Update components
    buttons.update();
    statusLed.update();
    display.update();
    wifiManager.update();
    audioOutput.update();  // Handle async audio playback

    // Check for wake word trigger
    if (wakeWordTriggered && currentState == AssistantState::IDLE) {
        wakeWordTriggered = false;
        Serial.println("[WakeWord] Triggered - starting voice input");
        wakeWord.stopListening();  // Stop wake word to free I2S for recording
        audioOutput.playStartSound();
        audioInput.startRecording();
        setState(AssistantState::LISTENING);
    }

    // Process audio if listening
    if (currentState == AssistantState::LISTENING) {
        audioInput.process();

        // Check for voice activity
        if (audioInput.isRecording()) {
            if (!audioInput.detectVoice()) {
                // Silence detected, stop recording
                Serial.println("[Voice] Silence detected, stopping...");
                audioInput.stopRecording();
                audioOutput.playStopSound();
                setState(AssistantState::PROCESSING);
            }
        }
    }

    // State machine processing
    switch (currentState) {
        case AssistantState::PROCESSING:
            processVoiceInput();
            break;

        case AssistantState::RESPONDING:
            // Check if audio playback finished
            if (!audioOutput.isPlaying()) {
                Serial.println("[Voice] Response playback complete");
                setState(AssistantState::IDLE);
            }
            break;

        case AssistantState::ERROR:
            // Auto-recover after 5 seconds
            static uint32_t errorTime = 0;
            if (errorTime == 0) errorTime = millis();
            if (millis() - errorTime > 5000) {
                errorTime = 0;
                if (wifiManager.isConnected()) {
                    setState(AssistantState::IDLE);
                }
            }
            break;

        default:
            break;
    }

    // Small delay to prevent watchdog issues
    delay(10);
}

void setState(AssistantState newState) {
    if (currentState == newState) return;

    Serial.printf("[State] %d -> %d\n", (int)currentState, (int)newState);
    currentState = newState;

    switch (newState) {
        case AssistantState::CONNECTING_WIFI:
            statusLed.setConnecting();
            display.setAssistantState(Display::AssistantState::IDLE);
            break;

        case AssistantState::IDLE:
            statusLed.setIdle();
            display.setAssistantState(Display::AssistantState::IDLE);
            display.updateStatusBar(wifiManager.getRSSI(), currentVolume, false);
            // Start wake word detection when idle
            if (WAKE_WORD_ENABLED && wakeWord.isEnabled()) {
                wakeWord.startListening();
            }
            break;

        case AssistantState::LISTENING:
            statusLed.setListening();
            display.setAssistantState(Display::AssistantState::LISTENING);
            display.updateStatusBar(wifiManager.getRSSI(), currentVolume, true);
            // Stop wake word detection during recording
            wakeWord.stopListening();
            break;

        case AssistantState::PROCESSING:
            statusLed.setThinking();
            display.setAssistantState(Display::AssistantState::THINKING);
            display.showThinking();
            break;

        case AssistantState::RESPONDING:
            statusLed.setSpeaking();
            display.setAssistantState(Display::AssistantState::SPEAKING);
            break;

        case AssistantState::ERROR:
            statusLed.setError();
            display.setAssistantState(Display::AssistantState::ERROR);
            display.showError(lastError);
            break;

        default:
            break;
    }
}

void handleButtonEvent(Button button, ButtonEvent event) {
    switch (button) {
        case Button::BOOT:
            if (event == ButtonEvent::PRESSED) {
                if (currentState == AssistantState::IDLE) {
                    // Start listening
                    Serial.println("[Button] Starting voice input...");
                    wakeWord.stopListening();  // Stop wake word to free I2S
                    audioOutput.playStartSound();
                    audioInput.startRecording();
                    setState(AssistantState::LISTENING);
                } else if (currentState == AssistantState::LISTENING) {
                    // Stop listening manually
                    Serial.println("[Button] Stopping voice input...");
                    audioInput.stopRecording();
                    audioOutput.playStopSound();
                    setState(AssistantState::PROCESSING);
                } else if (currentState == AssistantState::RESPONDING) {
                    // Interrupt response
                    Serial.println("[Button] Interrupting response...");
                    audioOutput.stop();
                    setState(AssistantState::IDLE);
                }
            }
            break;

        case Button::VOL_UP:
            if (event == ButtonEvent::PRESSED || event == ButtonEvent::LONG_PRESS) {
                currentVolume = min(currentVolume + VOLUME_STEP, MAX_VOLUME);
                audioOutput.setVolume(currentVolume);
                audioOutput.playBeep();
                display.updateStatusBar(wifiManager.getRSSI(), currentVolume,
                    currentState == AssistantState::LISTENING);
                Serial.printf("[Volume] %d%%\n", currentVolume);
            }
            break;

        case Button::VOL_DOWN:
            if (event == ButtonEvent::PRESSED || event == ButtonEvent::LONG_PRESS) {
                currentVolume = max(currentVolume - VOLUME_STEP, MIN_VOLUME);
                audioOutput.setVolume(currentVolume);
                if (currentVolume > 0) audioOutput.playBeep();
                display.updateStatusBar(wifiManager.getRSSI(), currentVolume,
                    currentState == AssistantState::LISTENING);
                Serial.printf("[Volume] %d%%\n", currentVolume);
            }
            break;
    }
}

void processVoiceInput() {
    // Get recorded audio info
    size_t audioSamples = audioInput.getBufferSize();
    Serial.printf("[Voice] Processing %d samples\n", audioSamples);

    if (audioSamples < 1000) {
        // Not enough audio captured
        Serial.println("[Voice] Too short, ignoring...");
        setState(AssistantState::IDLE);
        return;
    }

    // Step 1: Speech-to-Text - Convert audio to text
    Serial.println("[STT] Transcribing audio...");
    display.showThinking();

    String userText = speech.transcribe(
        audioInput.getBuffer(),
        audioSamples,
        I2S_MIC_SAMPLE_RATE
    );

    if (speech.hasError()) {
        lastError = "STT Error: " + speech.getLastError();
        Serial.println("[STT] " + lastError);
        setState(AssistantState::ERROR);
        audioOutput.playErrorSound();
        return;
    }

    if (userText.length() == 0) {
        Serial.println("[STT] No speech detected");
        setState(AssistantState::IDLE);
        return;
    }

    // Show user message on display
    display.showUserMessage(userText);
    Serial.println("[User] " + userText);

    // Step 2: Send to Gemini for AI response
    Serial.println("[Gemini] Sending request...");
    String response = gemini.chat(userText);

    if (gemini.hasError()) {
        lastError = gemini.getLastError();
        setState(AssistantState::ERROR);
        audioOutput.playErrorSound();
        return;
    }

    // Show AI response on display
    display.showAIMessage(response);
    Serial.println("[AI] " + response);

    // Step 3: Text-to-Speech - Convert response to audio
    Serial.println("[TTS] Synthesizing speech...");
    setState(AssistantState::RESPONDING);

    if (ttsBuffer) {
        size_t ttsSamples = speech.synthesize(
            response,
            ttsBuffer,
            ttsBufferSize,
            I2S_SPK_SAMPLE_RATE
        );

        if (speech.hasError()) {
            Serial.println("[TTS] Error: " + speech.getLastError());
            // Fall back to just showing text
            delay(2000);  // Give time to read
            setState(AssistantState::IDLE);
            return;
        }

        if (ttsSamples > 0) {
            Serial.printf("[TTS] Playing %d samples\n", ttsSamples);
            audioOutput.playAsync(ttsBuffer, ttsSamples);
            // State will change to IDLE when playback completes (in loop)
        } else {
            Serial.println("[TTS] No audio generated");
            delay(2000);
            setState(AssistantState::IDLE);
        }
    } else {
        // No TTS buffer, just show text
        Serial.println("[TTS] No buffer available, text-only mode");
        delay(2000);
        setState(AssistantState::IDLE);
    }
}
