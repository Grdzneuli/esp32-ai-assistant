/**
 * Unit tests for Audio utilities
 * Tests volume control, tone generation, and audio level calculations
 */

#include <unity.h>
#include <cmath>
#include <cstring>
#include <vector>

#ifdef NATIVE_BUILD
#include "../mocks/Arduino.h"
#endif

// ============================================================================
// Constants (from config.h)
// ============================================================================

#define MIN_VOLUME 0
#define MAX_VOLUME 100
#define DEFAULT_VOLUME 70

#define VAD_THRESHOLD 500
#define VAD_SILENCE_MS 1500

#define SAMPLE_RATE 16000

// ============================================================================
// Extracted Functions for Testing
// ============================================================================

// Apply volume to audio samples (from audio_output.cpp)
void applyVolume(int16_t* samples, size_t count, int volume) {
    float volumeMultiplier = volume / 100.0f;

    for (size_t i = 0; i < count; i++) {
        samples[i] = (int16_t)(samples[i] * volumeMultiplier);
    }
}

// Constrain volume to valid range
int constrainVolume(int volume) {
    return constrain(volume, MIN_VOLUME, MAX_VOLUME);
}

// Generate tone samples (from audio_output.cpp)
std::vector<int16_t> generateTone(int frequency, int durationMs, int sampleRate) {
    int sampleCount = (sampleRate * durationMs) / 1000;
    std::vector<int16_t> samples(sampleCount);

    float phase = 0;
    float phaseIncrement = (2.0f * M_PI * frequency) / sampleRate;

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

    return samples;
}

// Calculate average audio level (from audio_input.cpp)
int calculateAverageLevel(const int16_t* samples, size_t count) {
    if (count == 0) return 0;

    int32_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += abs(samples[i]);
    }
    return sum / count;
}

// Voice Activity Detection logic (from audio_input.cpp)
bool isVoiceDetected(int avgLevel, int threshold) {
    return avgLevel > threshold;
}

// Check if silence duration exceeded
bool isSilenceTimeout(uint32_t lastSoundTime, uint32_t currentTime, uint32_t silenceMs) {
    return (currentTime - lastSoundTime) > silenceMs;
}

// ============================================================================
// Volume Control Tests
// ============================================================================

void test_apply_volume_100_percent() {
    int16_t samples[] = {1000, -1000, 500, -500, 0};
    int16_t expected[] = {1000, -1000, 500, -500, 0};

    applyVolume(samples, 5, 100);

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_INT16(expected[i], samples[i]);
    }
}

void test_apply_volume_50_percent() {
    int16_t samples[] = {1000, -1000, 500, -500, 0};
    int16_t expected[] = {500, -500, 250, -250, 0};

    applyVolume(samples, 5, 50);

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_INT16(expected[i], samples[i]);
    }
}

void test_apply_volume_0_percent() {
    int16_t samples[] = {1000, -1000, 32767, -32768, 100};

    applyVolume(samples, 5, 0);

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_INT16(0, samples[i]);
    }
}

void test_apply_volume_25_percent() {
    int16_t samples[] = {1000, -1000, 400, -400};
    int16_t expected[] = {250, -250, 100, -100};

    applyVolume(samples, 4, 25);

    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT16(expected[i], samples[i]);
    }
}

void test_constrain_volume_within_range() {
    TEST_ASSERT_EQUAL(50, constrainVolume(50));
    TEST_ASSERT_EQUAL(0, constrainVolume(0));
    TEST_ASSERT_EQUAL(100, constrainVolume(100));
}

void test_constrain_volume_below_min() {
    TEST_ASSERT_EQUAL(0, constrainVolume(-10));
    TEST_ASSERT_EQUAL(0, constrainVolume(-100));
}

void test_constrain_volume_above_max() {
    TEST_ASSERT_EQUAL(100, constrainVolume(150));
    TEST_ASSERT_EQUAL(100, constrainVolume(1000));
}

// ============================================================================
// Tone Generation Tests
// ============================================================================

void test_generate_tone_correct_length() {
    auto samples = generateTone(1000, 100, SAMPLE_RATE);  // 100ms at 16kHz

    int expectedCount = (SAMPLE_RATE * 100) / 1000;  // 1600 samples
    TEST_ASSERT_EQUAL(expectedCount, samples.size());
}

void test_generate_tone_has_fade_in() {
    auto samples = generateTone(1000, 100, SAMPLE_RATE);

    // First samples should be near zero (fade in)
    TEST_ASSERT_TRUE(abs(samples[0]) < 1000);
    TEST_ASSERT_TRUE(abs(samples[10]) < abs(samples[100]));
}

void test_generate_tone_has_fade_out() {
    auto samples = generateTone(1000, 100, SAMPLE_RATE);
    size_t len = samples.size();

    // Last samples should be near zero (fade out)
    TEST_ASSERT_TRUE(abs(samples[len - 1]) < 1000);
    TEST_ASSERT_TRUE(abs(samples[len - 10]) < abs(samples[len - 100]));
}

void test_generate_tone_middle_has_amplitude() {
    auto samples = generateTone(1000, 100, SAMPLE_RATE);
    size_t mid = samples.size() / 2;

    // Find max amplitude in middle section
    int16_t maxAmp = 0;
    for (size_t i = mid - 50; i < mid + 50; i++) {
        if (abs(samples[i]) > maxAmp) {
            maxAmp = abs(samples[i]);
        }
    }

    // Middle should have significant amplitude
    TEST_ASSERT_TRUE(maxAmp > 10000);
}

void test_generate_tone_different_frequencies() {
    auto tone440 = generateTone(440, 100, SAMPLE_RATE);
    auto tone880 = generateTone(880, 100, SAMPLE_RATE);

    // Both should have same length
    TEST_ASSERT_EQUAL(tone440.size(), tone880.size());

    // Count zero crossings to verify different frequencies
    int crossings440 = 0;
    int crossings880 = 0;

    for (size_t i = 1; i < tone440.size(); i++) {
        if ((tone440[i-1] >= 0 && tone440[i] < 0) ||
            (tone440[i-1] < 0 && tone440[i] >= 0)) {
            crossings440++;
        }
        if ((tone880[i-1] >= 0 && tone880[i] < 0) ||
            (tone880[i-1] < 0 && tone880[i] >= 0)) {
            crossings880++;
        }
    }

    // 880Hz should have roughly twice as many zero crossings as 440Hz
    TEST_ASSERT_TRUE(crossings880 > crossings440 * 1.5);
    TEST_ASSERT_TRUE(crossings880 < crossings440 * 2.5);
}

void test_generate_tone_zero_duration() {
    auto samples = generateTone(1000, 0, SAMPLE_RATE);
    TEST_ASSERT_EQUAL(0, samples.size());
}

// ============================================================================
// Audio Level Calculation Tests
// ============================================================================

void test_average_level_silence() {
    int16_t samples[] = {0, 0, 0, 0, 0};
    int level = calculateAverageLevel(samples, 5);
    TEST_ASSERT_EQUAL(0, level);
}

void test_average_level_constant_positive() {
    int16_t samples[] = {100, 100, 100, 100, 100};
    int level = calculateAverageLevel(samples, 5);
    TEST_ASSERT_EQUAL(100, level);
}

void test_average_level_constant_negative() {
    int16_t samples[] = {-100, -100, -100, -100, -100};
    int level = calculateAverageLevel(samples, 5);
    TEST_ASSERT_EQUAL(100, level);  // Absolute value
}

void test_average_level_mixed() {
    int16_t samples[] = {100, -100, 200, -200, 0};
    int level = calculateAverageLevel(samples, 5);
    // (100 + 100 + 200 + 200 + 0) / 5 = 120
    TEST_ASSERT_EQUAL(120, level);
}

void test_average_level_empty() {
    int16_t samples[] = {};
    int level = calculateAverageLevel(samples, 0);
    TEST_ASSERT_EQUAL(0, level);
}

void test_average_level_max_amplitude() {
    int16_t samples[] = {32767, -32768, 32767, -32768};
    int level = calculateAverageLevel(samples, 4);
    // Should be close to 32767
    TEST_ASSERT_TRUE(level > 32000);
}

// ============================================================================
// Voice Activity Detection Tests
// ============================================================================

void test_vad_below_threshold() {
    TEST_ASSERT_FALSE(isVoiceDetected(400, VAD_THRESHOLD));
    TEST_ASSERT_FALSE(isVoiceDetected(0, VAD_THRESHOLD));
}

void test_vad_above_threshold() {
    TEST_ASSERT_TRUE(isVoiceDetected(600, VAD_THRESHOLD));
    TEST_ASSERT_TRUE(isVoiceDetected(1000, VAD_THRESHOLD));
}

void test_vad_at_threshold() {
    TEST_ASSERT_FALSE(isVoiceDetected(500, VAD_THRESHOLD));  // Equal is not above
}

void test_silence_timeout_not_exceeded() {
    TEST_ASSERT_FALSE(isSilenceTimeout(1000, 2000, VAD_SILENCE_MS));  // 1000ms < 1500ms
    TEST_ASSERT_FALSE(isSilenceTimeout(1000, 2500, VAD_SILENCE_MS));  // 1500ms not exceeded yet
}

void test_silence_timeout_exceeded() {
    TEST_ASSERT_TRUE(isSilenceTimeout(1000, 3000, VAD_SILENCE_MS));   // 2000ms > 1500ms
    TEST_ASSERT_TRUE(isSilenceTimeout(0, 5000, VAD_SILENCE_MS));       // 5000ms > 1500ms
}

void test_silence_timeout_edge_case() {
    // Exactly at boundary
    TEST_ASSERT_FALSE(isSilenceTimeout(1000, 2500, VAD_SILENCE_MS));  // 1500ms = 1500ms, not exceeded
    TEST_ASSERT_TRUE(isSilenceTimeout(1000, 2501, VAD_SILENCE_MS));   // 1501ms > 1500ms
}

// ============================================================================
// Buffer Management Tests
// ============================================================================

void test_buffer_copy_preserves_data() {
    int16_t source[] = {100, 200, 300, 400, 500};
    int16_t dest[5] = {0};

    memcpy(dest, source, 5 * sizeof(int16_t));

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL_INT16(source[i], dest[i]);
    }
}

void test_buffer_partial_copy() {
    int16_t source[] = {100, 200, 300, 400, 500};
    int16_t dest[10] = {0};

    // Copy first 3 elements
    memcpy(dest, source, 3 * sizeof(int16_t));

    TEST_ASSERT_EQUAL_INT16(100, dest[0]);
    TEST_ASSERT_EQUAL_INT16(200, dest[1]);
    TEST_ASSERT_EQUAL_INT16(300, dest[2]);
    TEST_ASSERT_EQUAL_INT16(0, dest[3]);  // Not copied
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Volume control tests
    RUN_TEST(test_apply_volume_100_percent);
    RUN_TEST(test_apply_volume_50_percent);
    RUN_TEST(test_apply_volume_0_percent);
    RUN_TEST(test_apply_volume_25_percent);
    RUN_TEST(test_constrain_volume_within_range);
    RUN_TEST(test_constrain_volume_below_min);
    RUN_TEST(test_constrain_volume_above_max);

    // Tone generation tests
    RUN_TEST(test_generate_tone_correct_length);
    RUN_TEST(test_generate_tone_has_fade_in);
    RUN_TEST(test_generate_tone_has_fade_out);
    RUN_TEST(test_generate_tone_middle_has_amplitude);
    RUN_TEST(test_generate_tone_different_frequencies);
    RUN_TEST(test_generate_tone_zero_duration);

    // Audio level tests
    RUN_TEST(test_average_level_silence);
    RUN_TEST(test_average_level_constant_positive);
    RUN_TEST(test_average_level_constant_negative);
    RUN_TEST(test_average_level_mixed);
    RUN_TEST(test_average_level_empty);
    RUN_TEST(test_average_level_max_amplitude);

    // VAD tests
    RUN_TEST(test_vad_below_threshold);
    RUN_TEST(test_vad_above_threshold);
    RUN_TEST(test_vad_at_threshold);
    RUN_TEST(test_silence_timeout_not_exceeded);
    RUN_TEST(test_silence_timeout_exceeded);
    RUN_TEST(test_silence_timeout_edge_case);

    // Buffer tests
    RUN_TEST(test_buffer_copy_preserves_data);
    RUN_TEST(test_buffer_partial_copy);

    return UNITY_END();
}
