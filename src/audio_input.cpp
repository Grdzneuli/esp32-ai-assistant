#include "audio_input.h"
#include "config.h"

#define MAX_RECORDING_SECONDS 10
#define SAMPLE_BUFFER_SIZE    512

AudioInput::AudioInput()
    : _initialized(false)
    , _recording(false)
    , _buffer(nullptr)
    , _bufferSize(0)
    , _bufferPos(0)
    , _readBuffer(nullptr)
    , _readBufferSize(SAMPLE_BUFFER_SIZE)
    , _callback(nullptr)
    , _lastSoundTime(0)
    , _avgLevel(0)
{
}

AudioInput::~AudioInput() {
    end();
}

bool AudioInput::begin() {
    if (_initialized) return true;

    // Allocate buffers in PSRAM if available
    _bufferSize = I2S_MIC_SAMPLE_RATE * MAX_RECORDING_SECONDS;

    if (psramFound()) {
        _buffer = (int16_t*)ps_malloc(_bufferSize * sizeof(int16_t));
        _readBuffer = (int16_t*)ps_malloc(_readBufferSize * sizeof(int16_t));
    } else {
        _buffer = (int16_t*)malloc(_bufferSize * sizeof(int16_t));
        _readBuffer = (int16_t*)malloc(_readBufferSize * sizeof(int16_t));
    }

    if (!_buffer || !_readBuffer) {
        Serial.println("[AudioInput] Failed to allocate buffers");
        return false;
    }

    if (!configureI2S()) {
        Serial.println("[AudioInput] Failed to configure I2S");
        return false;
    }

    _initialized = true;
    Serial.println("[AudioInput] Initialized");
    return true;
}

void AudioInput::end() {
    if (_initialized) {
        i2s_driver_uninstall(I2S_MIC_PORT);

        if (_buffer) {
            free(_buffer);
            _buffer = nullptr;
        }
        if (_readBuffer) {
            free(_readBuffer);
            _readBuffer = nullptr;
        }

        _initialized = false;
    }
}

bool AudioInput::configureI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_MIC_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
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

    esp_err_t err = i2s_driver_install(I2S_MIC_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[AudioInput] i2s_driver_install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_MIC_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[AudioInput] i2s_set_pin failed: %d\n", err);
        i2s_driver_uninstall(I2S_MIC_PORT);
        return false;
    }

    i2s_zero_dma_buffer(I2S_MIC_PORT);

    return true;
}

void AudioInput::startRecording() {
    if (!_initialized) return;

    clearBuffer();
    _recording = true;
    _lastSoundTime = millis();

    Serial.println("[AudioInput] Recording started");
}

void AudioInput::stopRecording() {
    _recording = false;
    Serial.printf("[AudioInput] Recording stopped, %d samples\n", _bufferPos);
}

void AudioInput::clearBuffer() {
    _bufferPos = 0;
    memset(_buffer, 0, _bufferSize * sizeof(int16_t));
}

void AudioInput::process() {
    if (!_initialized) return;

    size_t bytesRead = 0;
    esp_err_t result = i2s_read(
        I2S_MIC_PORT,
        _readBuffer,
        _readBufferSize * sizeof(int16_t),
        &bytesRead,
        pdMS_TO_TICKS(10)
    );

    if (result != ESP_OK || bytesRead == 0) return;

    size_t samplesRead = bytesRead / sizeof(int16_t);

    // Calculate average level for VAD
    int32_t sum = 0;
    for (size_t i = 0; i < samplesRead; i++) {
        sum += abs(_readBuffer[i]);
    }
    _avgLevel = sum / samplesRead;

    // Copy to main buffer if recording
    if (_recording) {
        size_t spaceLeft = _bufferSize - _bufferPos;
        size_t toCopy = min(samplesRead, spaceLeft);

        if (toCopy > 0) {
            memcpy(_buffer + _bufferPos, _readBuffer, toCopy * sizeof(int16_t));
            _bufferPos += toCopy;
        }

        // Auto-stop if buffer full
        if (_bufferPos >= _bufferSize) {
            stopRecording();
        }
    }

    // Call callback if set
    if (_callback) {
        _callback(_readBuffer, samplesRead);
    }
}

bool AudioInput::detectVoice() {
    if (_avgLevel > VAD_THRESHOLD) {
        _lastSoundTime = millis();
        return true;
    }

    // Check if silence duration exceeded
    if (_recording && (millis() - _lastSoundTime > VAD_SILENCE_MS)) {
        return false;  // Silence detected, should stop recording
    }

    return _recording;  // Continue if still recording
}

int AudioInput::getAverageLevel() {
    return _avgLevel;
}
