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
#include "display.h"
#include "audio_input.h"
#include "audio_output.h"
#include "buttons.h"
#include "led.h"

// Global objects
WiFiManager wifiManager;
GeminiClient gemini;
Display display;
AudioInput audioInput;
AudioOutput audioOutput;
Buttons buttons;
StatusLED statusLed;

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
            break;

        case AssistantState::LISTENING:
            statusLed.setListening();
            display.setAssistantState(Display::AssistantState::LISTENING);
            display.updateStatusBar(wifiManager.getRSSI(), currentVolume, true);
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
    // For now, we'll use a placeholder since full speech-to-text requires
    // additional cloud API integration. In a complete implementation,
    // you would send the audio buffer to Google Speech-to-Text API.

    // Get recorded audio info
    size_t audioSamples = audioInput.getBufferSize();
    Serial.printf("[Voice] Processing %d samples\n", audioSamples);

    if (audioSamples < 1000) {
        // Not enough audio captured
        Serial.println("[Voice] Too short, ignoring...");
        setState(AssistantState::IDLE);
        return;
    }

    // In a real implementation, you would:
    // 1. Encode the audio buffer to a format like WAV or FLAC
    // 2. Send it to Google Cloud Speech-to-Text API
    // 3. Get the transcribed text back
    //
    // For demonstration, we'll use a test message
    String userText = "Hello, what can you help me with?";

    // Show user message on display
    display.showUserMessage(userText);
    Serial.println("[User] " + userText);

    // Send to Gemini
    Serial.println("[Gemini] Sending request...");
    String response = gemini.chat(userText);

    if (gemini.hasError()) {
        lastError = gemini.getLastError();
        setState(AssistantState::ERROR);
        audioOutput.playErrorSound();
        return;
    }

    // Show AI response
    display.showAIMessage(response);
    Serial.println("[AI] " + response);

    // In a complete implementation, you would:
    // 1. Send the response text to Google Cloud Text-to-Speech API
    // 2. Receive audio data back
    // 3. Play the audio through the speaker
    //
    // For demonstration, we just show the text and play a sound
    setState(AssistantState::RESPONDING);
    audioOutput.playBeep();
    delay(500);

    setState(AssistantState::IDLE);
}

// Helper function to convert audio buffer to text (placeholder)
String getTextFromAudio() {
    // This would integrate with Google Cloud Speech-to-Text API
    // For now, return empty string
    return "";
}
