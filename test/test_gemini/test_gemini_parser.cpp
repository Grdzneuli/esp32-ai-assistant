/**
 * Unit tests for Gemini API response parsing
 * Tests JSON parsing logic extracted from GeminiClient
 */

#include <unity.h>
#include <ArduinoJson.h>

#ifdef NATIVE_BUILD
#include "../mocks/Arduino.h"
#endif

// ============================================================================
// Helper functions (extracted from gemini_client.cpp for testing)
// ============================================================================

String parseGeminiResponse(const String& response) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response.c_str());

    if (error) {
        return String("JSON Error: ") + error.c_str();
    }

    // Check for API error
    if (doc["error"].is<JsonObject>()) {
        String errorMsg = doc["error"]["message"] | "Unknown error";
        return String("API Error: ") + errorMsg;
    }

    // Extract response text from candidates
    if (doc["candidates"].is<JsonArray>() && doc["candidates"].size() > 0) {
        JsonObject candidate = doc["candidates"][0];
        if (candidate["content"].is<JsonObject>()) {
            JsonArray parts = candidate["content"]["parts"];
            if (parts.size() > 0) {
                String text = parts[0]["text"] | "";
                return text;
            }
        }
    }

    return "";
}

String extractFinishReason(const String& response) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response.c_str());

    if (error) return "";

    if (doc["candidates"].is<JsonArray>() && doc["candidates"].size() > 0) {
        return doc["candidates"][0]["finishReason"] | "";
    }
    return "";
}

// ============================================================================
// Test Cases
// ============================================================================

void test_parse_valid_simple_response() {
    const char* json = R"({
        "candidates": [{
            "content": {
                "parts": [{"text": "Hello! How can I help you today?"}]
            },
            "finishReason": "STOP"
        }]
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("Hello! How can I help you today?", result.c_str());
}

void test_parse_valid_multiline_response() {
    const char* json = R"({
        "candidates": [{
            "content": {
                "parts": [{"text": "Here are the steps:\n1. First step\n2. Second step\n3. Third step"}]
            },
            "finishReason": "STOP"
        }]
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_TRUE(result.indexOf("1. First step") >= 0);
    TEST_ASSERT_TRUE(result.indexOf("2. Second step") >= 0);
    TEST_ASSERT_TRUE(result.indexOf("3. Third step") >= 0);
}

void test_parse_response_with_special_characters() {
    const char* json = R"({
        "candidates": [{
            "content": {
                "parts": [{"text": "Temperature is 25\u00b0C and humidity is 60%"}]
            }
        }]
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_TRUE(result.length() > 0);
    TEST_ASSERT_TRUE(result.indexOf("Temperature") >= 0);
}

void test_parse_empty_response() {
    const char* json = R"({
        "candidates": [{
            "content": {
                "parts": [{"text": ""}]
            }
        }]
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_parse_api_error_invalid_key() {
    const char* json = R"({
        "error": {
            "code": 400,
            "message": "API key not valid. Please pass a valid API key.",
            "status": "INVALID_ARGUMENT"
        }
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_TRUE(result.indexOf("API Error") >= 0);
    TEST_ASSERT_TRUE(result.indexOf("API key not valid") >= 0);
}

void test_parse_api_error_quota_exceeded() {
    const char* json = R"({
        "error": {
            "code": 429,
            "message": "Resource has been exhausted",
            "status": "RESOURCE_EXHAUSTED"
        }
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_TRUE(result.indexOf("API Error") >= 0);
    TEST_ASSERT_TRUE(result.indexOf("exhausted") >= 0);
}

void test_parse_malformed_json() {
    const char* json = "{ invalid json }}}";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_TRUE(result.indexOf("JSON Error") >= 0);
}

void test_parse_empty_json() {
    const char* json = "{}";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_parse_missing_candidates() {
    const char* json = R"({
        "someOtherField": "value"
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_parse_empty_candidates_array() {
    const char* json = R"({
        "candidates": []
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_parse_missing_content() {
    const char* json = R"({
        "candidates": [{
            "finishReason": "STOP"
        }]
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_parse_missing_parts() {
    const char* json = R"({
        "candidates": [{
            "content": {}
        }]
    })";

    String result = parseGeminiResponse(String(json));
    TEST_ASSERT_EQUAL_STRING("", result.c_str());
}

void test_extract_finish_reason_stop() {
    const char* json = R"({
        "candidates": [{
            "content": {"parts": [{"text": "Response"}]},
            "finishReason": "STOP"
        }]
    })";

    String reason = extractFinishReason(String(json));
    TEST_ASSERT_EQUAL_STRING("STOP", reason.c_str());
}

void test_extract_finish_reason_max_tokens() {
    const char* json = R"({
        "candidates": [{
            "content": {"parts": [{"text": "Truncated..."}]},
            "finishReason": "MAX_TOKENS"
        }]
    })";

    String reason = extractFinishReason(String(json));
    TEST_ASSERT_EQUAL_STRING("MAX_TOKENS", reason.c_str());
}

void test_extract_finish_reason_safety() {
    const char* json = R"({
        "candidates": [{
            "content": {"parts": [{"text": ""}]},
            "finishReason": "SAFETY"
        }]
    })";

    String reason = extractFinishReason(String(json));
    TEST_ASSERT_EQUAL_STRING("SAFETY", reason.c_str());
}

void test_parse_long_response() {
    // Create a long response (simulating verbose AI output)
    String longText = "";
    for (int i = 0; i < 100; i++) {
        longText += "This is sentence number " + String(i) + ". ";
    }

    String json = R"({"candidates": [{"content": {"parts": [{"text": ")" + longText + R"("}]}}]})";

    String result = parseGeminiResponse(json);
    TEST_ASSERT_TRUE(result.length() > 1000);
    TEST_ASSERT_TRUE(result.indexOf("sentence number 0") >= 0);
    TEST_ASSERT_TRUE(result.indexOf("sentence number 99") >= 0);
}

// ============================================================================
// Test Runner
// ============================================================================

void setUp() {
    // Setup before each test
}

void tearDown() {
    // Cleanup after each test
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Valid response tests
    RUN_TEST(test_parse_valid_simple_response);
    RUN_TEST(test_parse_valid_multiline_response);
    RUN_TEST(test_parse_response_with_special_characters);
    RUN_TEST(test_parse_empty_response);
    RUN_TEST(test_parse_long_response);

    // Error response tests
    RUN_TEST(test_parse_api_error_invalid_key);
    RUN_TEST(test_parse_api_error_quota_exceeded);
    RUN_TEST(test_parse_malformed_json);

    // Edge case tests
    RUN_TEST(test_parse_empty_json);
    RUN_TEST(test_parse_missing_candidates);
    RUN_TEST(test_parse_empty_candidates_array);
    RUN_TEST(test_parse_missing_content);
    RUN_TEST(test_parse_missing_parts);

    // Finish reason tests
    RUN_TEST(test_extract_finish_reason_stop);
    RUN_TEST(test_extract_finish_reason_max_tokens);
    RUN_TEST(test_extract_finish_reason_safety);

    return UNITY_END();
}
