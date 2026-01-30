#ifndef WAKE_WORD_H
#define WAKE_WORD_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>

// Wake word detection callback
typedef void (*WakeWordCallback)(void);

class WakeWordDetector {
public:
    WakeWordDetector();
    ~WakeWordDetector();

    // Initialize the wake word detector
    bool begin();

    // Stop the wake word detector
    void end();

    // Start listening for wake word
    void startListening();

    // Stop listening (when main recording takes over)
    void stopListening();

    // Check if currently listening
    bool isListening() const { return _listening; }

    // Set callback for wake word detection
    void setCallback(WakeWordCallback callback) { _callback = callback; }

    // Adjust detection sensitivity (0.0 - 1.0)
    void setSensitivity(float sensitivity);

    // Enable/disable wake word detection
    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled() const { return _enabled; }

    // Get detection statistics
    int getDetectionCount() const { return _detectionCount; }

private:
    // FreeRTOS task for background audio processing
    static void detectionTask(void* param);

    // Audio processing methods
    void processAudioFrame(int16_t* samples, size_t count);
    float calculateEnergy(int16_t* samples, size_t count);
    float calculateZeroCrossingRate(int16_t* samples, size_t count);
    bool detectWakePattern();

    // I2S configuration (shares mic with main audio)
    bool configureI2S();
    void releaseI2S();

    // State
    bool _initialized;
    bool _listening;
    bool _enabled;
    WakeWordCallback _callback;

    // Detection task
    TaskHandle_t _taskHandle;
    volatile bool _taskRunning;

    // Audio buffer
    int16_t* _audioBuffer;
    static const size_t FRAME_SIZE = 512;
    static const size_t BUFFER_FRAMES = 16;  // ~0.5 seconds at 16kHz

    // Feature buffers for pattern detection
    float* _energyHistory;
    float* _zcrHistory;
    size_t _historyIndex;
    static const size_t HISTORY_SIZE = 32;

    // Detection parameters
    float _sensitivity;
    float _energyThreshold;
    float _triggerThreshold;

    // Pattern state machine
    enum class PatternState {
        IDLE,
        RISING_EDGE,
        SUSTAINED,
        FALLING_EDGE,
        DETECTED
    };
    PatternState _patternState;
    uint32_t _patternStartTime;
    int _sustainedFrames;

    // Statistics
    int _detectionCount;
    uint32_t _lastDetectionTime;
    static const uint32_t DETECTION_COOLDOWN_MS = 2000;
};

#endif // WAKE_WORD_H
