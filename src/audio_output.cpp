#include "audio_output.h"
#include "config.h"
#include <cmath>

AudioOutput::AudioOutput()
    : _initialized(false)
    , _playing(false)
    , _volume(DEFAULT_VOLUME)
{
}

AudioOutput::~AudioOutput() {
    end();
}

bool AudioOutput::begin() {
    if (_initialized) return true;

    if (!configureI2S()) {
        Serial.println("[AudioOutput] Failed to configure I2S");
        return false;
    }

    _initialized = true;
    Serial.println("[AudioOutput] Initialized");
    return true;
}

void AudioOutput::end() {
    if (_initialized) {
        i2s_driver_uninstall(I2S_SPK_PORT);
        _initialized = false;
    }
}

bool AudioOutput::configureI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SPK_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SPK_BCLK_PIN,
        .ws_io_num = I2S_SPK_LRCLK_PIN,
        .data_out_num = I2S_SPK_DIN_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_SPK_PORT, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[AudioOutput] i2s_driver_install failed: %d\n", err);
        return false;
    }

    err = i2s_set_pin(I2S_SPK_PORT, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[AudioOutput] i2s_set_pin failed: %d\n", err);
        i2s_driver_uninstall(I2S_SPK_PORT);
        return false;
    }

    i2s_zero_dma_buffer(I2S_SPK_PORT);

    return true;
}

void AudioOutput::setVolume(int volume) {
    _volume = constrain(volume, MIN_VOLUME, MAX_VOLUME);
    Serial.printf("[AudioOutput] Volume set to %d%%\n", _volume);
}

void AudioOutput::play(const int16_t* samples, size_t count) {
    if (!_initialized || count == 0) return;

    _playing = true;

    // Create a copy to apply volume
    int16_t* buffer = (int16_t*)malloc(count * sizeof(int16_t));
    if (!buffer) {
        Serial.println("[AudioOutput] Failed to allocate buffer");
        _playing = false;
        return;
    }

    memcpy(buffer, samples, count * sizeof(int16_t));
    applyVolume(buffer, count);

    size_t bytesWritten = 0;
    i2s_write(I2S_SPK_PORT, buffer, count * sizeof(int16_t), &bytesWritten, portMAX_DELAY);

    free(buffer);
    _playing = false;
}

void AudioOutput::playTone(int frequency, int durationMs) {
    if (!_initialized) return;

    _playing = true;

    int sampleCount = (I2S_SPK_SAMPLE_RATE * durationMs) / 1000;
    int16_t* samples = (int16_t*)malloc(sampleCount * sizeof(int16_t));

    if (!samples) {
        Serial.println("[AudioOutput] Failed to allocate tone buffer");
        _playing = false;
        return;
    }

    // Generate sine wave
    float phase = 0;
    float phaseIncrement = (2.0f * M_PI * frequency) / I2S_SPK_SAMPLE_RATE;

    for (int i = 0; i < sampleCount; i++) {
        // Apply envelope for smoother sound
        float envelope = 1.0f;
        int fadeLen = sampleCount / 10;

        if (i < fadeLen) {
            envelope = (float)i / fadeLen;  // Fade in
        } else if (i > sampleCount - fadeLen) {
            envelope = (float)(sampleCount - i) / fadeLen;  // Fade out
        }

        samples[i] = (int16_t)(sin(phase) * 16000 * envelope);
        phase += phaseIncrement;
        if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
    }

    play(samples, sampleCount);
    free(samples);

    _playing = false;
}

void AudioOutput::playBeep() {
    playTone(1000, 50);  // 1kHz for 50ms
}

void AudioOutput::playStartSound() {
    playTone(800, 100);
    delay(50);
    playTone(1200, 100);
}

void AudioOutput::playStopSound() {
    playTone(1200, 100);
    delay(50);
    playTone(800, 100);
}

void AudioOutput::playErrorSound() {
    playTone(400, 200);
    delay(100);
    playTone(300, 300);
}

void AudioOutput::stop() {
    if (_initialized) {
        i2s_zero_dma_buffer(I2S_SPK_PORT);
        _playing = false;
    }
}

void AudioOutput::applyVolume(int16_t* samples, size_t count) {
    float volumeMultiplier = _volume / 100.0f;

    for (size_t i = 0; i < count; i++) {
        samples[i] = (int16_t)(samples[i] * volumeMultiplier);
    }
}
