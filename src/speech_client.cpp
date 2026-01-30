#include "speech_client.h"
#include "config.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// Base64 encoding table
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

SpeechClient::SpeechClient()
    : _languageCode("en-US")
    , _voiceName("en-US-Neural2-A")
    , _hasError(false)
{
}

void SpeechClient::begin(const char* apiKey) {
    _apiKey = apiKey;
    Serial.println("[SpeechClient] Initialized");
}

void SpeechClient::setError(const String& error) {
    _hasError = true;
    _lastError = error;
    Serial.println("[SpeechClient] Error: " + error);
}

void SpeechClient::clearError() {
    _hasError = false;
    _lastError = "";
}

String SpeechClient::base64Encode(const uint8_t* data, size_t length) {
    String encoded;
    encoded.reserve((length + 2) / 3 * 4);

    int i = 0;
    uint8_t byte3[3];
    uint8_t byte4[4];

    while (length--) {
        byte3[i++] = *(data++);
        if (i == 3) {
            byte4[0] = (byte3[0] & 0xfc) >> 2;
            byte4[1] = ((byte3[0] & 0x03) << 4) + ((byte3[1] & 0xf0) >> 4);
            byte4[2] = ((byte3[1] & 0x0f) << 2) + ((byte3[2] & 0xc0) >> 6);
            byte4[3] = byte3[2] & 0x3f;

            for (i = 0; i < 4; i++) {
                encoded += base64_chars[byte4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; j++) {
            byte3[j] = 0;
        }

        byte4[0] = (byte3[0] & 0xfc) >> 2;
        byte4[1] = ((byte3[0] & 0x03) << 4) + ((byte3[1] & 0xf0) >> 4);
        byte4[2] = ((byte3[1] & 0x0f) << 2) + ((byte3[2] & 0xc0) >> 6);
        byte4[3] = byte3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++) {
            encoded += base64_chars[byte4[j]];
        }

        while (i++ < 3) {
            encoded += '=';
        }
    }

    return encoded;
}

size_t SpeechClient::base64Decode(const String& input, uint8_t* output, size_t maxLength) {
    size_t inputLen = input.length();
    if (inputLen % 4 != 0) return 0;

    size_t outputLen = inputLen / 4 * 3;
    if (input[inputLen - 1] == '=') outputLen--;
    if (input[inputLen - 2] == '=') outputLen--;

    if (outputLen > maxLength) return 0;

    size_t j = 0;
    for (size_t i = 0; i < inputLen;) {
        uint32_t sextet_a = input[i] == '=' ? 0 : strchr(base64_chars, input[i]) - base64_chars;
        uint32_t sextet_b = input[i+1] == '=' ? 0 : strchr(base64_chars, input[i+1]) - base64_chars;
        uint32_t sextet_c = input[i+2] == '=' ? 0 : strchr(base64_chars, input[i+2]) - base64_chars;
        uint32_t sextet_d = input[i+3] == '=' ? 0 : strchr(base64_chars, input[i+3]) - base64_chars;

        uint32_t triple = (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;

        if (j < outputLen) output[j++] = (triple >> 16) & 0xFF;
        if (j < outputLen) output[j++] = (triple >> 8) & 0xFF;
        if (j < outputLen) output[j++] = triple & 0xFF;

        i += 4;
    }

    return outputLen;
}

String SpeechClient::transcribe(const int16_t* audioBuffer, size_t sampleCount, int sampleRate) {
    clearError();

    if (!audioBuffer || sampleCount == 0) {
        setError("Invalid audio buffer");
        return "";
    }

    Serial.printf("[SpeechClient] Transcribing %d samples at %d Hz\n", sampleCount, sampleRate);

    // Encode audio as base64
    String audioContent = base64Encode((const uint8_t*)audioBuffer, sampleCount * sizeof(int16_t));

    Serial.printf("[SpeechClient] Encoded audio size: %d bytes\n", audioContent.length());

    // Build request JSON
    JsonDocument doc;

    JsonObject config = doc["config"].to<JsonObject>();
    config["encoding"] = "LINEAR16";
    config["sampleRateHertz"] = sampleRate;
    config["languageCode"] = _languageCode;
    config["enableAutomaticPunctuation"] = true;
    config["model"] = "latest_short";

    JsonObject audio = doc["audio"].to<JsonObject>();
    audio["content"] = audioContent;

    String requestBody;
    serializeJson(doc, requestBody);

    // Make HTTP request
    WiFiClientSecure client;
    client.setInsecure();  // Skip certificate verification for simplicity

    HTTPClient http;
    String url = "https://speech.googleapis.com/v1/speech:recognize?key=" + _apiKey;

    Serial.println("[SpeechClient] Sending request to Speech-to-Text API...");

    if (!http.begin(client, url)) {
        setError("Failed to connect to Speech API");
        return "";
    }

    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000);  // 30 second timeout

    int httpCode = http.POST(requestBody);

    if (httpCode <= 0) {
        setError("HTTP request failed: " + String(http.errorToString(httpCode).c_str()));
        http.end();
        return "";
    }

    String response = http.getString();
    http.end();

    Serial.printf("[SpeechClient] Response code: %d\n", httpCode);

    if (httpCode != 200) {
        setError("API error: " + response.substring(0, 200));
        return "";
    }

    // Parse response
    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
        setError("Failed to parse response: " + String(error.c_str()));
        return "";
    }

    // Extract transcript
    if (responseDoc["results"].size() > 0) {
        JsonObject firstResult = responseDoc["results"][0];
        if (firstResult["alternatives"].size() > 0) {
            String transcript = firstResult["alternatives"][0]["transcript"].as<String>();
            Serial.println("[SpeechClient] Transcript: " + transcript);
            return transcript;
        }
    }

    Serial.println("[SpeechClient] No speech detected");
    return "";
}

size_t SpeechClient::getEstimatedSamples(const String& text, int sampleRate) {
    // Rough estimate: about 150 words per minute speaking rate
    // Average 5 characters per word
    // So characters / 5 = words, words / 150 = minutes
    // minutes * 60 * sampleRate = samples
    // Simplified: characters * sampleRate * 60 / (5 * 150) = characters * sampleRate * 0.08
    // Add 50% buffer for safety
    return (size_t)(text.length() * sampleRate * 0.12);
}

size_t SpeechClient::synthesize(const String& text, int16_t* outputBuffer, size_t maxSamples, int sampleRate) {
    clearError();

    if (text.length() == 0) {
        setError("Empty text");
        return 0;
    }

    if (!outputBuffer || maxSamples == 0) {
        setError("Invalid output buffer");
        return 0;
    }

    Serial.println("[SpeechClient] Synthesizing: " + text.substring(0, 50) + "...");

    // Build request JSON
    JsonDocument doc;

    JsonObject input = doc["input"].to<JsonObject>();
    input["text"] = text;

    JsonObject voice = doc["voice"].to<JsonObject>();
    voice["languageCode"] = _languageCode.substring(0, 5);  // e.g., "en-US"
    voice["name"] = _voiceName;

    JsonObject audioConfig = doc["audioConfig"].to<JsonObject>();
    audioConfig["audioEncoding"] = "LINEAR16";
    audioConfig["sampleRateHertz"] = sampleRate;
    audioConfig["speakingRate"] = 1.0;

    String requestBody;
    serializeJson(doc, requestBody);

    // Make HTTP request
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://texttospeech.googleapis.com/v1/text:synthesize?key=" + _apiKey;

    Serial.println("[SpeechClient] Sending request to Text-to-Speech API...");

    if (!http.begin(client, url)) {
        setError("Failed to connect to TTS API");
        return 0;
    }

    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000);

    int httpCode = http.POST(requestBody);

    if (httpCode <= 0) {
        setError("HTTP request failed: " + String(http.errorToString(httpCode).c_str()));
        http.end();
        return 0;
    }

    String response = http.getString();
    http.end();

    Serial.printf("[SpeechClient] Response code: %d\n", httpCode);

    if (httpCode != 200) {
        setError("API error: " + response.substring(0, 200));
        return 0;
    }

    // Parse response
    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);

    if (error) {
        setError("Failed to parse response: " + String(error.c_str()));
        return 0;
    }

    // Get audio content (base64 encoded)
    String audioContent = responseDoc["audioContent"].as<String>();

    if (audioContent.length() == 0) {
        setError("No audio content in response");
        return 0;
    }

    Serial.printf("[SpeechClient] Received audio, base64 size: %d\n", audioContent.length());

    // Decode base64 to PCM
    // LINEAR16 response includes a 44-byte WAV header, we need to skip it
    size_t maxBytes = maxSamples * sizeof(int16_t) + 44;
    uint8_t* tempBuffer = (uint8_t*)malloc(maxBytes);

    if (!tempBuffer) {
        setError("Failed to allocate decode buffer");
        return 0;
    }

    size_t decodedBytes = base64Decode(audioContent, tempBuffer, maxBytes);

    if (decodedBytes == 0) {
        free(tempBuffer);
        setError("Failed to decode audio");
        return 0;
    }

    Serial.printf("[SpeechClient] Decoded %d bytes\n", decodedBytes);

    // Skip WAV header (44 bytes) if present
    size_t offset = 0;
    if (decodedBytes > 44 && tempBuffer[0] == 'R' && tempBuffer[1] == 'I' &&
        tempBuffer[2] == 'F' && tempBuffer[3] == 'F') {
        offset = 44;
        Serial.println("[SpeechClient] Skipping WAV header");
    }

    // Copy PCM data
    size_t pcmBytes = decodedBytes - offset;
    size_t samples = pcmBytes / sizeof(int16_t);

    if (samples > maxSamples) {
        samples = maxSamples;
        Serial.println("[SpeechClient] Warning: Audio truncated to fit buffer");
    }

    memcpy(outputBuffer, tempBuffer + offset, samples * sizeof(int16_t));
    free(tempBuffer);

    Serial.printf("[SpeechClient] Synthesized %d samples\n", samples);

    return samples;
}
