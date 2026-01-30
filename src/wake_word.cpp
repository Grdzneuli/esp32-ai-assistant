#include "wake_word.h"
#include "config.h"
#include <cmath>

WakeWordDetector::WakeWordDetector()
    : _initialized(false)
    , _listening(false)
    , _enabled(true)
    , _callback(nullptr)
    , _taskHandle(nullptr)
    , _taskRunning(false)
    , _audioBuffer(nullptr)
    , _energyHistory(nullptr)
    , _zcrHistory(nullptr)
    , _historyIndex(0)
    , _sensitivity(0.5f)
    , _energyThreshold(WAKE_WORD_ENERGY_THRESHOLD)
    , _triggerThreshold(WAKE_WORD_TRIGGER_THRESHOLD)
    , _patternState(PatternState::IDLE)
    , _patternStartTime(0)
    , _sustainedFrames(0)
    , _detectionCount(0)
    , _lastDetectionTime(0)
{
}

WakeWordDetector::~WakeWordDetector() {
    end();
}

bool WakeWordDetector::begin() {
    if (_initialized) return true;

    // Allocate audio buffer in PSRAM if available
    if (psramFound()) {
        _audioBuffer = (int16_t*)ps_malloc(FRAME_SIZE * sizeof(int16_t));
        _energyHistory = (float*)ps_malloc(HISTORY_SIZE * sizeof(float));
        _zcrHistory = (float*)ps_malloc(HISTORY_SIZE * sizeof(float));
    } else {
        _audioBuffer = (int16_t*)malloc(FRAME_SIZE * sizeof(int16_t));
        _energyHistory = (float*)malloc(HISTORY_SIZE * sizeof(float));
        _zcrHistory = (float*)malloc(HISTORY_SIZE * sizeof(float));
    }

    if (!_audioBuffer || !_energyHistory || !_zcrHistory) {
        Serial.println("[WakeWord] Failed to allocate buffers");
        end();
        return false;
    }

    // Initialize history buffers
    memset(_energyHistory, 0, HISTORY_SIZE * sizeof(float));
    memset(_zcrHistory, 0, HISTORY_SIZE * sizeof(float));

    _initialized = true;
    Serial.println("[WakeWord] Initialized");
    Serial.printf("[WakeWord] Sensitivity: %.2f, Energy threshold: %.0f\n",
                  _sensitivity, _energyThreshold);

    return true;
}

void WakeWordDetector::end() {
    stopListening();

    if (_audioBuffer) {
        free(_audioBuffer);
        _audioBuffer = nullptr;
    }
    if (_energyHistory) {
        free(_energyHistory);
        _energyHistory = nullptr;
    }
    if (_zcrHistory) {
        free(_zcrHistory);
        _zcrHistory = nullptr;
    }

    _initialized = false;
}

void WakeWordDetector::startListening() {
    if (!_initialized || _listening || !_enabled) return;

    // Configure I2S for wake word detection
    if (!configureI2S()) {
        Serial.println("[WakeWord] Failed to configure I2S");
        return;
    }

    // Reset detection state
    _patternState = PatternState::IDLE;
    _historyIndex = 0;
    _sustainedFrames = 0;
    memset(_energyHistory, 0, HISTORY_SIZE * sizeof(float));
    memset(_zcrHistory, 0, HISTORY_SIZE * sizeof(float));

    // Create detection task
    _taskRunning = true;
    xTaskCreatePinnedToCore(
        detectionTask,
        "wake_word",
        4096,
        this,
        1,  // Low priority
        &_taskHandle,
        0   // Core 0 (let main app run on Core 1)
    );

    _listening = true;
    Serial.println("[WakeWord] Started listening");
}

void WakeWordDetector::stopListening() {
    if (!_listening) return;

    // Stop the detection task
    _taskRunning = false;
    if (_taskHandle) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Give task time to exit
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }

    // Release I2S
    releaseI2S();

    _listening = false;
    Serial.println("[WakeWord] Stopped listening");
}

void WakeWordDetector::setSensitivity(float sensitivity) {
    _sensitivity = constrain(sensitivity, 0.0f, 1.0f);

    // Adjust thresholds based on sensitivity
    // Higher sensitivity = lower thresholds = easier to trigger
    float factor = 1.0f + (1.0f - _sensitivity);  // 1.0 to 2.0
    _energyThreshold = WAKE_WORD_ENERGY_THRESHOLD / factor;
    _triggerThreshold = WAKE_WORD_TRIGGER_THRESHOLD / factor;

    Serial.printf("[WakeWord] Sensitivity: %.2f, Thresholds: %.0f / %.2f\n",
                  _sensitivity, _energyThreshold, _triggerThreshold);
}

bool WakeWordDetector::configureI2S() {
    // Note: I2S mic may already be configured by AudioInput
    // We'll use the same port but need to ensure it's available

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_MIC_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = FRAME_SIZE,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_MIC_SCK_PIN,
        .ws_io_num = I2S_MIC_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD_PIN
    };

    // Try to install driver (may already be installed)
    esp_err_t err = i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        Serial.printf("[WakeWord] i2s_driver_install failed: %d\n", err);
        return false;
    }

    // Set pins (always needed)
    err = i2s_set_pin(I2S_MIC_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[WakeWord] i2s_set_pin failed: %d\n", err);
        return false;
    }

    return true;
}

void WakeWordDetector::releaseI2S() {
    // Don't uninstall driver - it may be shared with AudioInput
    // Just stop reading
    i2s_stop(I2S_MIC_PORT);
}

void WakeWordDetector::detectionTask(void* param) {
    WakeWordDetector* detector = (WakeWordDetector*)param;

    Serial.println("[WakeWord] Detection task started");

    while (detector->_taskRunning) {
        // Read audio frame from I2S
        size_t bytesRead = 0;
        esp_err_t err = i2s_read(
            I2S_MIC_PORT,
            detector->_audioBuffer,
            FRAME_SIZE * sizeof(int16_t),
            &bytesRead,
            pdMS_TO_TICKS(100)
        );

        if (err == ESP_OK && bytesRead > 0) {
            size_t samplesRead = bytesRead / sizeof(int16_t);
            detector->processAudioFrame(detector->_audioBuffer, samplesRead);
        }

        // Small delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    Serial.println("[WakeWord] Detection task stopped");
    vTaskDelete(NULL);
}

void WakeWordDetector::processAudioFrame(int16_t* samples, size_t count) {
    // Calculate audio features
    float energy = calculateEnergy(samples, count);
    float zcr = calculateZeroCrossingRate(samples, count);

    // Store in history buffer
    _energyHistory[_historyIndex] = energy;
    _zcrHistory[_historyIndex] = zcr;
    _historyIndex = (_historyIndex + 1) % HISTORY_SIZE;

    // Check for wake word pattern
    if (detectWakePattern()) {
        // Check cooldown
        uint32_t now = millis();
        if (now - _lastDetectionTime >= DETECTION_COOLDOWN_MS) {
            _lastDetectionTime = now;
            _detectionCount++;
            Serial.println("[WakeWord] Wake word detected!");

            // Trigger callback
            if (_callback) {
                _callback();
            }
        }
    }
}

float WakeWordDetector::calculateEnergy(int16_t* samples, size_t count) {
    float sum = 0;
    for (size_t i = 0; i < count; i++) {
        float normalized = samples[i] / 32768.0f;
        sum += normalized * normalized;
    }
    return sqrt(sum / count) * 32768.0f;  // RMS energy
}

float WakeWordDetector::calculateZeroCrossingRate(int16_t* samples, size_t count) {
    int crossings = 0;
    for (size_t i = 1; i < count; i++) {
        if ((samples[i-1] >= 0 && samples[i] < 0) ||
            (samples[i-1] < 0 && samples[i] >= 0)) {
            crossings++;
        }
    }
    return (float)crossings / count;
}

bool WakeWordDetector::detectWakePattern() {
    // Calculate recent average energy
    float avgEnergy = 0;
    for (size_t i = 0; i < HISTORY_SIZE; i++) {
        avgEnergy += _energyHistory[i];
    }
    avgEnergy /= HISTORY_SIZE;

    // Calculate recent average ZCR
    float avgZcr = 0;
    for (size_t i = 0; i < HISTORY_SIZE; i++) {
        avgZcr += _zcrHistory[i];
    }
    avgZcr /= HISTORY_SIZE;

    // Get current frame values
    size_t currentIdx = (_historyIndex + HISTORY_SIZE - 1) % HISTORY_SIZE;
    float currentEnergy = _energyHistory[currentIdx];
    float currentZcr = _zcrHistory[currentIdx];

    // Pattern detection state machine
    // Looking for: silence -> voiced speech (low ZCR, high energy) -> silence
    // This pattern matches typical wake words like "Hey", "Hello", "OK"

    switch (_patternState) {
        case PatternState::IDLE:
            // Look for rising edge (energy increase)
            if (currentEnergy > _energyThreshold &&
                currentEnergy > avgEnergy * 1.5f) {
                _patternState = PatternState::RISING_EDGE;
                _patternStartTime = millis();
                _sustainedFrames = 0;
            }
            break;

        case PatternState::RISING_EDGE:
            // Look for sustained voiced speech
            // Voice has moderate ZCR (0.05-0.15) and high energy
            if (currentEnergy > _energyThreshold * 0.8f &&
                currentZcr > 0.02f && currentZcr < 0.2f) {
                _sustainedFrames++;
                if (_sustainedFrames >= 3) {
                    _patternState = PatternState::SUSTAINED;
                }
            } else if (currentEnergy < _energyThreshold * 0.3f) {
                // Dropped too fast, reset
                _patternState = PatternState::IDLE;
            }
            break;

        case PatternState::SUSTAINED:
            // Continue tracking voiced speech
            if (currentEnergy > _energyThreshold * 0.5f) {
                _sustainedFrames++;

                // Check for maximum duration (wake words are typically 0.3-1.0 sec)
                uint32_t elapsed = millis() - _patternStartTime;
                if (elapsed > 1500) {
                    // Too long, probably not a wake word
                    _patternState = PatternState::IDLE;
                }
            } else {
                // Energy dropped, look for falling edge
                _patternState = PatternState::FALLING_EDGE;
            }
            break;

        case PatternState::FALLING_EDGE:
            // Confirm pattern completion
            {
                uint32_t elapsed = millis() - _patternStartTime;

                // Valid wake word pattern:
                // - Duration: 300ms - 1200ms
                // - Sustained frames: at least 5
                if (elapsed >= 300 && elapsed <= 1200 && _sustainedFrames >= 5) {
                    _patternState = PatternState::DETECTED;
                    return true;
                } else {
                    _patternState = PatternState::IDLE;
                }
            }
            break;

        case PatternState::DETECTED:
            // Reset after detection
            _patternState = PatternState::IDLE;
            break;
    }

    return false;
}
