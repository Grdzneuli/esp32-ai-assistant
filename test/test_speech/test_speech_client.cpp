/**
 * Unit tests for Speech client functionality
 * Tests base64 encoding, STT response parsing, TTS response parsing
 */

#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>
#include <vector>

#ifdef NATIVE_BUILD
#include "../mocks/Arduino.h"
#endif

// ============================================================================
// Base64 encoding implementation (extracted from speech_client.cpp)
// ============================================================================

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64Encode(const uint8_t* data, size_t length) {
    String result;
    result._str.reserve(((length + 2) / 3) * 4);

    int i = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (int j = 0; j < 4; j++) {
                result += base64_chars[char_array_4[j]];
            }
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (int j = 0; j < i + 1; j++) {
            result += base64_chars[char_array_4[j]];
        }

        while (i++ < 3) {
            result += '=';
        }
    }

    return result;
}

std::vector<uint8_t> base64Decode(const String& encoded) {
    std::vector<uint8_t> result;

    int in_len = encoded.length();
    int i = 0;
    int in_ = 0;
    uint8_t char_array_4[4], char_array_3[3];

    while (in_len-- && encoded[in_] != '=') {
        char c = encoded[in_];
        const char* pos = strchr(base64_chars, c);
        if (pos == nullptr) {
            in_++;
            continue;
        }

        char_array_4[i++] = pos - base64_chars;
        in_++;

        if (i == 4) {
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (int j = 0; j < 3; j++) {
                result.push_back(char_array_3[j]);
            }
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 4; j++) {
            char_array_4[j] = 0;
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (int j = 0; j < i - 1; j++) {
            result.push_back(char_array_3[j]);
        }
    }

    return result;
}

// ============================================================================
// STT Response parsing (extracted from speech_client.cpp)
// ============================================================================

String parseSTTResponse(const String& response) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response.c_str());

    if (error) {
        return "";
    }

    // Check for error
    if (doc["error"].is<JsonObject>()) {
        return "";
    }

    // Extract transcript from results
    if (doc["results"].is<JsonArray>() && doc["results"].size() > 0) {
        JsonArray alternatives = doc["results"][0]["alternatives"];
        if (alternatives.size() > 0) {
            return alternatives[0]["transcript"] | "";
        }
    }

    return "";
}

float parseSTTConfidence(const String& response) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response.c_str());

    if (error) return 0.0f;

    if (doc["results"].is<JsonArray>() && doc["results"].size() > 0) {
        JsonArray alternatives = doc["results"][0]["alternatives"];
        if (alternatives.size() > 0) {
            return alternatives[0]["confidence"] | 0.0f;
        }
    }

    return 0.0f;
}

// ============================================================================
// TTS Response parsing
// ============================================================================

String parseTTSAudioContent(const String& response) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response.c_str());

    if (error) return "";

    return doc["audioContent"] | "";
}

// ============================================================================
// Base64 Test Cases
// ============================================================================

void test_base64_encode_empty() {
    uint8_t data[] = {};
    String result = base64Encode(data, 0);
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_base64_encode_single_byte() {
    uint8_t data[] = {0x00};
    String result = base64Encode(data, 1);
    TEST_ASSERT_EQUAL_STRING("AA==", result.c_str());
}

void test_base64_encode_two_bytes() {
    uint8_t data[] = {0x00, 0x00};
    String result = base64Encode(data, 2);
    TEST_ASSERT_EQUAL_STRING("AAA=", result.c_str());
}

void test_base64_encode_three_bytes() {
    uint8_t data[] = {0x00, 0x00, 0x00};
    String result = base64Encode(data, 3);
    TEST_ASSERT_EQUAL_STRING("AAAA", result.c_str());
}

void test_base64_encode_hello() {
    const char* text = "Hello";
    String result = base64Encode((const uint8_t*)text, strlen(text));
    TEST_ASSERT_EQUAL_STRING("SGVsbG8=", result.c_str());
}

void test_base64_encode_hello_world() {
    const char* text = "Hello, World!";
    String result = base64Encode((const uint8_t*)text, strlen(text));
    TEST_ASSERT_EQUAL_STRING("SGVsbG8sIFdvcmxkIQ==", result.c_str());
}

void test_base64_encode_binary_data() {
    uint8_t data[] = {0xFF, 0x00, 0xAA, 0x55, 0x12, 0x34};
    String result = base64Encode(data, sizeof(data));
    TEST_ASSERT_EQUAL_STRING("/wCqVRI0", result.c_str());
}

void test_base64_decode_empty() {
    std::vector<uint8_t> result = base64Decode(String(""));
    TEST_ASSERT_EQUAL(0, result.size());
}

void test_base64_decode_hello() {
    std::vector<uint8_t> result = base64Decode(String("SGVsbG8="));
    TEST_ASSERT_EQUAL(5, result.size());
    TEST_ASSERT_EQUAL_STRING("Hello", std::string(result.begin(), result.end()).c_str());
}

void test_base64_roundtrip() {
    const char* original = "Test data for roundtrip encoding!";
    String encoded = base64Encode((const uint8_t*)original, strlen(original));
    std::vector<uint8_t> decoded = base64Decode(encoded);

    TEST_ASSERT_EQUAL(strlen(original), decoded.size());
    TEST_ASSERT_EQUAL_MEMORY(original, decoded.data(), decoded.size());
}

void test_base64_roundtrip_binary() {
    uint8_t original[] = {0x00, 0x01, 0x02, 0xFE, 0xFF, 0x80, 0x7F};
    String encoded = base64Encode(original, sizeof(original));
    std::vector<uint8_t> decoded = base64Decode(encoded);

    TEST_ASSERT_EQUAL(sizeof(original), decoded.size());
    TEST_ASSERT_EQUAL_MEMORY(original, decoded.data(), decoded.size());
}

// ============================================================================
// STT Response Test Cases
// ============================================================================

void test_stt_parse_valid_response() {
    const char* json = R"({
        "results": [{
            "alternatives": [{
                "transcript": "Hello, how are you?",
                "confidence": 0.95
            }]
        }]
    })";

    String result = parseSTTResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("Hello, how are you?", result.c_str());
}

void test_stt_parse_confidence() {
    const char* json = R"({
        "results": [{
            "alternatives": [{
                "transcript": "Test",
                "confidence": 0.87
            }]
        }]
    })";

    float confidence = parseSTTConfidence(String(json));
    TEST_ASSERT_FLOAT_WITHIN(0.01, 0.87, confidence);
}

void test_stt_parse_empty_results() {
    const char* json = R"({"results": []})";
    String result = parseSTTResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_stt_parse_no_results() {
    const char* json = R"({})";
    String result = parseSTTResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_stt_parse_error_response() {
    const char* json = R"({
        "error": {
            "code": 400,
            "message": "Invalid audio format"
        }
    })";

    String result = parseSTTResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_stt_parse_multiple_alternatives() {
    const char* json = R"({
        "results": [{
            "alternatives": [
                {"transcript": "First choice", "confidence": 0.9},
                {"transcript": "Second choice", "confidence": 0.7}
            ]
        }]
    })";

    String result = parseSTTResponse(String(json));
    // Should return first (highest confidence) alternative
    TEST_ASSERT_EQUAL_STRING("First choice", result.c_str());
}

// ============================================================================
// TTS Response Test Cases
// ============================================================================

void test_tts_parse_valid_response() {
    const char* json = R"({
        "audioContent": "SGVsbG8gV29ybGQ="
    })";

    String audioContent = parseTTSAudioContent(String(json));
    TEST_ASSERT_EQUAL_STRING("SGVsbG8gV29ybGQ=", audioContent.c_str());
}

void test_tts_parse_empty_response() {
    const char* json = R"({})";
    String audioContent = parseTTSAudioContent(String(json));
    TEST_ASSERT_EQUAL_STRING("", audioContent.c_str());
}

void test_tts_parse_error_response() {
    const char* json = R"({
        "error": {
            "code": 400,
            "message": "Invalid text input"
        }
    })";

    String audioContent = parseTTSAudioContent(String(json));
    TEST_ASSERT_EQUAL_STRING("", audioContent.c_str());
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp() {}
void tearDown() {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Base64 encoding tests
    RUN_TEST(test_base64_encode_empty);
    RUN_TEST(test_base64_encode_single_byte);
    RUN_TEST(test_base64_encode_two_bytes);
    RUN_TEST(test_base64_encode_three_bytes);
    RUN_TEST(test_base64_encode_hello);
    RUN_TEST(test_base64_encode_hello_world);
    RUN_TEST(test_base64_encode_binary_data);

    // Base64 decoding tests
    RUN_TEST(test_base64_decode_empty);
    RUN_TEST(test_base64_decode_hello);
    RUN_TEST(test_base64_roundtrip);
    RUN_TEST(test_base64_roundtrip_binary);

    // STT response parsing tests
    RUN_TEST(test_stt_parse_valid_response);
    RUN_TEST(test_stt_parse_confidence);
    RUN_TEST(test_stt_parse_empty_results);
    RUN_TEST(test_stt_parse_no_results);
    RUN_TEST(test_stt_parse_error_response);
    RUN_TEST(test_stt_parse_multiple_alternatives);

    // TTS response parsing tests
    RUN_TEST(test_tts_parse_valid_response);
    RUN_TEST(test_tts_parse_empty_response);
    RUN_TEST(test_tts_parse_error_response);

    return UNITY_END();
}
