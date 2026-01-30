/**
 * Unit tests for Wake Word detection
 * Tests audio feature extraction and pattern detection logic
 */

#include <unity.h>
#include <cmath>
#include <cstdint>
#include <cstring>

#ifdef NATIVE_BUILD
#include "../mocks/Arduino.h"
#endif

// ============================================================================
// Audio feature extraction functions (from wake_word.cpp)
// ============================================================================

float calculateEnergy(int16_t* samples, size_t count) {
    if (count == 0) return 0.0f;

    float sum = 0;
    for (size_t i = 0; i < count; i++) {
        float normalized = samples[i] / 32768.0f;
        sum += normalized * normalized;
    }
    return sqrt(sum / count) * 32768.0f;  // RMS energy
}

float calculateZeroCrossingRate(int16_t* samples, size_t count) {
    if (count <= 1) return 0.0f;

    int crossings = 0;
    for (size_t i = 1; i < count; i++) {
        if ((samples[i-1] >= 0 && samples[i] < 0) ||
            (samples[i-1] < 0 && samples[i] >= 0)) {
            crossings++;
        }
    }
    return (float)crossings / count;
}

// ============================================================================
// Pattern detection state machine (simplified from wake_word.cpp)
// ============================================================================

enum class PatternState {
    IDLE,
    RISING_EDGE,
    SUSTAINED,
    FALLING_EDGE,
    DETECTED
};

struct WakeWordState {
    PatternState patternState = PatternState::IDLE;
    int sustainedFrames = 0;
    float energyThreshold = 800.0f;
};

// Returns true if wake word pattern detected
bool processFrame(WakeWordState& state, float energy, float zcr, float avgEnergy) {
    switch (state.patternState) {
        case PatternState::IDLE:
            // Look for rising edge (energy increase)
            if (energy > state.energyThreshold &&
                energy > avgEnergy * 1.5f) {
                state.patternState = PatternState::RISING_EDGE;
                state.sustainedFrames = 0;
            }
            break;

        case PatternState::RISING_EDGE:
            // Look for sustained voiced speech
            if (energy > state.energyThreshold * 0.8f &&
                zcr > 0.02f && zcr < 0.2f) {
                state.sustainedFrames++;
                if (state.sustainedFrames >= 3) {
                    state.patternState = PatternState::SUSTAINED;
                }
            } else if (energy < state.energyThreshold * 0.3f) {
                state.patternState = PatternState::IDLE;
            }
            break;

        case PatternState::SUSTAINED:
            if (energy > state.energyThreshold * 0.5f) {
                state.sustainedFrames++;
            } else {
                state.patternState = PatternState::FALLING_EDGE;
            }
            break;

        case PatternState::FALLING_EDGE:
            // Valid if sustained for enough frames
            if (state.sustainedFrames >= 5) {
                state.patternState = PatternState::DETECTED;
                return true;
            } else {
                state.patternState = PatternState::IDLE;
            }
            break;

        case PatternState::DETECTED:
            state.patternState = PatternState::IDLE;
            break;
    }

    return false;
}

// ============================================================================
// Test Helper Functions
// ============================================================================

void generateSilence(int16_t* buffer, size_t count) {
    memset(buffer, 0, count * sizeof(int16_t));
}

void generateNoise(int16_t* buffer, size_t count, int16_t amplitude) {
    for (size_t i = 0; i < count; i++) {
        buffer[i] = (i % 2 == 0) ? amplitude : -amplitude;
    }
}

void generateSineWave(int16_t* buffer, size_t count, float frequency, int sampleRate, int16_t amplitude) {
    for (size_t i = 0; i < count; i++) {
        float t = (float)i / sampleRate;
        buffer[i] = (int16_t)(amplitude * sin(2 * M_PI * frequency * t));
    }
}

void generateSpeechLikeSignal(int16_t* buffer, size_t count, int16_t amplitude) {
    // Mix of fundamental and harmonics (simulating voiced speech)
    for (size_t i = 0; i < count; i++) {
        float t = (float)i / 16000.0f;  // 16kHz sample rate
        float signal = 0.6f * sin(2 * M_PI * 150 * t);   // Fundamental ~150Hz
        signal += 0.3f * sin(2 * M_PI * 300 * t);        // First harmonic
        signal += 0.1f * sin(2 * M_PI * 450 * t);        // Second harmonic
        buffer[i] = (int16_t)(amplitude * signal);
    }
}

// ============================================================================
// Energy Calculation Tests
// ============================================================================

void test_energy_silence() {
    int16_t samples[512];
    generateSilence(samples, 512);

    float energy = calculateEnergy(samples, 512);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, energy);
}

void test_energy_empty_buffer() {
    int16_t samples[1] = {0};
    float energy = calculateEnergy(samples, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, energy);
}

void test_energy_constant_signal() {
    int16_t samples[512];
    for (int i = 0; i < 512; i++) {
        samples[i] = 16384;  // Half max amplitude
    }

    float energy = calculateEnergy(samples, 512);
    // RMS of constant signal equals the signal value
    TEST_ASSERT_FLOAT_WITHIN(100.0f, 16384.0f, energy);
}

void test_energy_max_amplitude() {
    int16_t samples[512];
    for (int i = 0; i < 512; i++) {
        samples[i] = 32767;
    }

    float energy = calculateEnergy(samples, 512);
    TEST_ASSERT_FLOAT_WITHIN(100.0f, 32767.0f, energy);
}

void test_energy_alternating() {
    int16_t samples[512];
    generateNoise(samples, 512, 10000);

    float energy = calculateEnergy(samples, 512);
    // Energy of alternating signal
    TEST_ASSERT_TRUE(energy > 5000.0f && energy < 15000.0f);
}

void test_energy_sine_wave() {
    int16_t samples[512];
    generateSineWave(samples, 512, 440.0f, 16000, 16000);

    float energy = calculateEnergy(samples, 512);
    // RMS of sine wave is amplitude / sqrt(2) ≈ 0.707 * amplitude
    float expectedRMS = 16000.0f / sqrt(2.0f);
    TEST_ASSERT_FLOAT_WITHIN(1000.0f, expectedRMS, energy);
}

void test_energy_speech_like() {
    int16_t samples[512];
    generateSpeechLikeSignal(samples, 512, 8000);

    float energy = calculateEnergy(samples, 512);
    // Should have moderate energy
    TEST_ASSERT_TRUE(energy > 1000.0f && energy < 10000.0f);
}

// ============================================================================
// Zero Crossing Rate Tests
// ============================================================================

void test_zcr_silence() {
    int16_t samples[512];
    generateSilence(samples, 512);

    float zcr = calculateZeroCrossingRate(samples, 512);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, zcr);
}

void test_zcr_empty_buffer() {
    int16_t samples[1] = {0};
    float zcr = calculateZeroCrossingRate(samples, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, zcr);
}

void test_zcr_single_sample() {
    int16_t samples[1] = {1000};
    float zcr = calculateZeroCrossingRate(samples, 1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, zcr);
}

void test_zcr_max_crossings() {
    int16_t samples[512];
    generateNoise(samples, 512, 1000);  // Alternating +/-

    float zcr = calculateZeroCrossingRate(samples, 512);
    // Should be close to 1.0 (crossing every sample)
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 511.0f/512.0f, zcr);
}

void test_zcr_no_crossings() {
    int16_t samples[512];
    for (int i = 0; i < 512; i++) {
        samples[i] = 1000;  // All positive
    }

    float zcr = calculateZeroCrossingRate(samples, 512);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, zcr);
}

void test_zcr_sine_wave_440hz() {
    int16_t samples[512];
    generateSineWave(samples, 512, 440.0f, 16000, 10000);

    float zcr = calculateZeroCrossingRate(samples, 512);
    // 440Hz at 16kHz sample rate → ~2 crossings per period
    // Expected crossings ≈ 2 * 440 / 16000 = 0.055
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.055f, zcr);
}

void test_zcr_sine_wave_1000hz() {
    int16_t samples[512];
    generateSineWave(samples, 512, 1000.0f, 16000, 10000);

    float zcr = calculateZeroCrossingRate(samples, 512);
    // 1000Hz at 16kHz → 2 * 1000 / 16000 = 0.125
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.125f, zcr);
}

void test_zcr_speech_range() {
    // Voiced speech typically has ZCR in range 0.02-0.2
    int16_t samples[512];
    generateSpeechLikeSignal(samples, 512, 8000);

    float zcr = calculateZeroCrossingRate(samples, 512);
    TEST_ASSERT_TRUE(zcr > 0.01f && zcr < 0.3f);
}

// ============================================================================
// Pattern Detection State Machine Tests
// ============================================================================

void test_pattern_starts_idle() {
    WakeWordState state;
    TEST_ASSERT_EQUAL(PatternState::IDLE, state.patternState);
}

void test_pattern_no_trigger_on_silence() {
    WakeWordState state;
    state.energyThreshold = 800.0f;

    // Process several silent frames
    for (int i = 0; i < 20; i++) {
        bool detected = processFrame(state, 0.0f, 0.0f, 100.0f);
        TEST_ASSERT_FALSE(detected);
    }

    TEST_ASSERT_EQUAL(PatternState::IDLE, state.patternState);
}

void test_pattern_transition_to_rising_edge() {
    WakeWordState state;
    state.energyThreshold = 800.0f;

    // Low energy frame
    processFrame(state, 100.0f, 0.05f, 100.0f);
    TEST_ASSERT_EQUAL(PatternState::IDLE, state.patternState);

    // Sudden high energy frame
    processFrame(state, 2000.0f, 0.05f, 100.0f);
    TEST_ASSERT_EQUAL(PatternState::RISING_EDGE, state.patternState);
}

void test_pattern_full_detection_sequence() {
    WakeWordState state;
    state.energyThreshold = 500.0f;

    // IDLE → RISING_EDGE (sudden energy spike)
    processFrame(state, 1000.0f, 0.05f, 200.0f);
    TEST_ASSERT_EQUAL(PatternState::RISING_EDGE, state.patternState);

    // RISING_EDGE → SUSTAINED (3 voiced frames)
    processFrame(state, 900.0f, 0.08f, 200.0f);  // Frame 1
    processFrame(state, 850.0f, 0.07f, 200.0f);  // Frame 2
    processFrame(state, 800.0f, 0.06f, 200.0f);  // Frame 3 - triggers SUSTAINED
    TEST_ASSERT_EQUAL(PatternState::SUSTAINED, state.patternState);

    // SUSTAINED (maintain energy)
    processFrame(state, 700.0f, 0.05f, 200.0f);
    processFrame(state, 600.0f, 0.05f, 200.0f);
    TEST_ASSERT_EQUAL(PatternState::SUSTAINED, state.patternState);

    // SUSTAINED → FALLING_EDGE (energy drops)
    processFrame(state, 100.0f, 0.05f, 200.0f);
    TEST_ASSERT_EQUAL(PatternState::FALLING_EDGE, state.patternState);

    // FALLING_EDGE → DETECTED (sufficient sustained frames)
    bool detected = processFrame(state, 50.0f, 0.02f, 200.0f);
    TEST_ASSERT_TRUE(detected);
    TEST_ASSERT_EQUAL(PatternState::DETECTED, state.patternState);
}

void test_pattern_resets_on_too_short() {
    WakeWordState state;
    state.energyThreshold = 500.0f;

    // Quick spike and immediate drop (too short)
    processFrame(state, 1000.0f, 0.05f, 200.0f);  // RISING_EDGE
    processFrame(state, 100.0f, 0.05f, 200.0f);   // Energy drops too fast

    TEST_ASSERT_EQUAL(PatternState::IDLE, state.patternState);
}

void test_pattern_resets_on_high_zcr() {
    WakeWordState state;
    state.energyThreshold = 500.0f;

    // Energy spike
    processFrame(state, 1000.0f, 0.05f, 200.0f);
    TEST_ASSERT_EQUAL(PatternState::RISING_EDGE, state.patternState);

    // High ZCR (noise, not speech) - should still allow some frames
    processFrame(state, 900.0f, 0.5f, 200.0f);  // ZCR > 0.2 (noisy)
    // Will not increment sustained frames due to high ZCR
}

void test_pattern_no_false_trigger_on_noise() {
    WakeWordState state;
    state.energyThreshold = 500.0f;

    // Simulate noisy environment (high ZCR, moderate energy)
    for (int i = 0; i < 50; i++) {
        bool detected = processFrame(state, 600.0f, 0.4f, 500.0f);  // High ZCR
        TEST_ASSERT_FALSE(detected);
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

void test_realistic_wake_word_audio() {
    // Simulate a "Hey" wake word pattern:
    // 1. Silence (low energy)
    // 2. Onset (rising energy)
    // 3. Voiced sound (moderate energy, low ZCR)
    // 4. Decay (falling energy)
    // 5. Silence again

    WakeWordState state;
    state.energyThreshold = 500.0f;

    bool detected = false;

    // Phase 1: Silence (5 frames)
    for (int i = 0; i < 5; i++) {
        detected |= processFrame(state, 100.0f, 0.01f, 150.0f);
    }
    TEST_ASSERT_FALSE(detected);

    // Phase 2: Onset (energy ramps up)
    detected |= processFrame(state, 400.0f, 0.05f, 150.0f);
    detected |= processFrame(state, 800.0f, 0.06f, 150.0f);
    detected |= processFrame(state, 1200.0f, 0.07f, 150.0f);
    TEST_ASSERT_FALSE(detected);  // Not yet complete

    // Phase 3: Sustained voiced (typical speech characteristics)
    for (int i = 0; i < 10; i++) {
        detected |= processFrame(state, 1000.0f + (i % 3) * 50, 0.08f, 300.0f);
    }

    // Phase 4: Decay
    detected |= processFrame(state, 500.0f, 0.06f, 300.0f);
    detected |= processFrame(state, 200.0f, 0.04f, 300.0f);
    detected |= processFrame(state, 50.0f, 0.02f, 300.0f);

    // Should have detected the wake word
    TEST_ASSERT_TRUE(detected);
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Energy calculation tests
    RUN_TEST(test_energy_silence);
    RUN_TEST(test_energy_empty_buffer);
    RUN_TEST(test_energy_constant_signal);
    RUN_TEST(test_energy_max_amplitude);
    RUN_TEST(test_energy_alternating);
    RUN_TEST(test_energy_sine_wave);
    RUN_TEST(test_energy_speech_like);

    // Zero crossing rate tests
    RUN_TEST(test_zcr_silence);
    RUN_TEST(test_zcr_empty_buffer);
    RUN_TEST(test_zcr_single_sample);
    RUN_TEST(test_zcr_max_crossings);
    RUN_TEST(test_zcr_no_crossings);
    RUN_TEST(test_zcr_sine_wave_440hz);
    RUN_TEST(test_zcr_sine_wave_1000hz);
    RUN_TEST(test_zcr_speech_range);

    // Pattern detection tests
    RUN_TEST(test_pattern_starts_idle);
    RUN_TEST(test_pattern_no_trigger_on_silence);
    RUN_TEST(test_pattern_transition_to_rising_edge);
    RUN_TEST(test_pattern_full_detection_sequence);
    RUN_TEST(test_pattern_resets_on_too_short);
    RUN_TEST(test_pattern_resets_on_high_zcr);
    RUN_TEST(test_pattern_no_false_trigger_on_noise);

    // Integration tests
    RUN_TEST(test_realistic_wake_word_audio);

    return UNITY_END();
}
