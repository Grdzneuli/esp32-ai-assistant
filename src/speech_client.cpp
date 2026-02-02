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

// Helper to get base64 character value
static int base64CharValue(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return 0;  // Padding
    return -1;  // Invalid
}

// Robust base64 decode that handles whitespace and validates input
size_t SpeechClient::base64Decode(const char* input, size_t inputLen, uint8_t* output, size_t maxLength) {
    Serial.println("[BASE64] Step 1: Starting decode");
    Serial.printf("[BASE64] Input length: %d, Max output: %d\n", inputLen, maxLength);

    if (inputLen == 0) {
        Serial.println("[BASE64] Error: Empty input");
        return 0;
    }

    // Count valid base64 characters
    size_t validCount = 0;
    size_t paddingCount = 0;

    for (size_t i = 0; i < inputLen; i++) {
        char c = input[i];
        if (c == '=') {
            paddingCount++;
            validCount++;
        } else if (base64CharValue(c) >= 0) {
            validCount++;
        }
        // Skip whitespace and other chars
    }

    Serial.printf("[BASE64] Step 2: Found %d valid base64 chars, %d padding\n", validCount, paddingCount);

    // Adjust to multiple of 4 by truncating if necessary
    size_t adjustedLen = (validCount / 4) * 4;
    if (adjustedLen == 0) {
        Serial.println("[BASE64] Error: Not enough valid characters");
        return 0;
    }

    Serial.printf("[BASE64] Step 3: Adjusted length to %d (multiple of 4)\n", adjustedLen);

    // Calculate output size
    size_t outputLen = (adjustedLen / 4) * 3;
    // Account for padding
    if (paddingCount >= 1) outputLen--;
    if (paddingCount >= 2) outputLen--;

    Serial.printf("[BASE64] Step 4: Expected output size: %d bytes\n", outputLen);

    if (outputLen > maxLength) {
        Serial.printf("[BASE64] Error: Output too large (%d > %d)\n", outputLen, maxLength);
        return 0;
    }

    // Decode
    Serial.println("[BASE64] Step 5: Decoding...");
    size_t outPos = 0;
    size_t validIdx = 0;
    uint8_t quad[4];

    for (size_t i = 0; i < inputLen && validIdx < adjustedLen; i++) {
        char c = input[i];
        int val = base64CharValue(c);

        if (val < 0 && c != '=') continue;  // Skip invalid chars

        quad[validIdx % 4] = (c == '=') ? 0 : val;
        validIdx++;

        if (validIdx % 4 == 0) {
            // Decode quad
            if (outPos < maxLength) output[outPos++] = (quad[0] << 2) | (quad[1] >> 4);
            if (outPos < maxLength && outPos < outputLen) output[outPos++] = (quad[1] << 4) | (quad[2] >> 2);
            if (outPos < maxLength && outPos < outputLen) output[outPos++] = (quad[2] << 6) | quad[3];
        }
    }

    Serial.printf("[BASE64] Step 6: Decoded %d bytes\n", outPos);

    // Show first few decoded bytes
    if (outPos >= 8) {
        Serial.printf("[BASE64] First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                      output[0], output[1], output[2], output[3],
                      output[4], output[5], output[6], output[7]);
    }

    return outPos;
}

// Keep old signature for compatibility with transcribe
size_t SpeechClient::base64Decode(const String& input, uint8_t* output, size_t maxLength) {
    return base64Decode(input.c_str(), input.length(), output, maxLength);
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

    // Build request JSON manually (ArduinoJson can't handle 66KB+ strings)
    String requestBody;
    requestBody.reserve(audioContent.length() + 256);  // Pre-allocate

    requestBody = "{\"config\":{";
    requestBody += "\"encoding\":\"LINEAR16\",";
    requestBody += "\"sampleRateHertz\":" + String(sampleRate) + ",";
    requestBody += "\"languageCode\":\"" + _languageCode + "\",";
    requestBody += "\"enableAutomaticPunctuation\":true,";
    requestBody += "\"model\":\"latest_short\"";
    requestBody += "},\"audio\":{\"content\":\"";
    requestBody += audioContent;
    requestBody += "\"}}";

    Serial.printf("[SpeechClient] Request body size: %d bytes\n", requestBody.length());

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

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] STEP 1: VALIDATE INPUT");
    Serial.println("[TTS] ====================================================");

    if (text.length() == 0) {
        setError("Empty text");
        return 0;
    }

    if (!outputBuffer || maxSamples == 0) {
        setError("Invalid output buffer");
        return 0;
    }

    Serial.println("[TTS] Text to synthesize:");
    Serial.println(text);
    Serial.printf("[TTS] Text length: %d chars\n", text.length());
    Serial.printf("[TTS] Output buffer: %d max samples (%d bytes)\n", maxSamples, maxSamples * 2);
    Serial.println("[TTS] Input validation: OK");

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] STEP 2: BUILD HTTP REQUEST");
    Serial.println("[TTS] ====================================================");

    // Build request JSON
    JsonDocument doc;
    JsonObject input = doc["input"].to<JsonObject>();
    input["text"] = text;

    JsonObject voice = doc["voice"].to<JsonObject>();
    voice["languageCode"] = _languageCode.substring(0, 5);
    voice["name"] = _voiceName;

    JsonObject audioConfig = doc["audioConfig"].to<JsonObject>();
    audioConfig["audioEncoding"] = "LINEAR16";
    audioConfig["sampleRateHertz"] = sampleRate;

    String requestBody;
    serializeJson(doc, requestBody);

    Serial.printf("[TTS] Request body size: %d bytes\n", requestBody.length());
    Serial.println("[TTS] Request JSON: OK");

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] STEP 3: SEND HTTP REQUEST");
    Serial.println("[TTS] ====================================================");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = "https://texttospeech.googleapis.com/v1/text:synthesize?key=" + _apiKey;

    if (!http.begin(client, url)) {
        setError("Failed to connect to TTS API");
        return 0;
    }

    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000);

    Serial.println("[TTS] Sending POST request...");
    int httpCode = http.POST(requestBody);

    Serial.printf("[TTS] HTTP response code: %d\n", httpCode);

    if (httpCode <= 0) {
        setError("HTTP request failed: " + String(http.errorToString(httpCode).c_str()));
        http.end();
        return 0;
    }

    if (httpCode != 200) {
        String errorResp = http.getString();
        setError("API error: " + errorResp.substring(0, 200));
        http.end();
        return 0;
    }

    Serial.println("[TTS] HTTP request: OK");

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] STEP 4: READ RESPONSE");
    Serial.println("[TTS] ====================================================");

    int contentLength = http.getSize();
    Serial.printf("[TTS] Content-Length: %d\n", contentLength);

    // Use getString() which properly handles chunked transfer encoding
    // For TTS responses under ~1MB this works reliably
    Serial.println("[TTS] Reading response with getString()...");
    unsigned long startTime = millis();

    String response = http.getString();
    http.end();

    size_t totalRead = response.length();
    Serial.printf("[TTS] Total bytes read: %d\n", totalRead);
    Serial.printf("[TTS] Read time: %d ms\n", millis() - startTime);

    if (totalRead < 50) {
        setError("Response too short");
        return 0;
    }

    // Show first 100 chars of response
    Serial.println("[TTS] Response preview (first 100 chars):");
    Serial.println(response.substring(0, 100));

    Serial.println("[TTS] Response read: OK");

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] STEP 5: EXTRACT BASE64 DATA");
    Serial.println("[TTS] ====================================================");

    // Find "audioContent": marker
    int markerPos = response.indexOf("\"audioContent\":");
    if (markerPos < 0) {
        markerPos = response.indexOf("\"audioContent\": ");
    }

    if (markerPos < 0) {
        setError("No audioContent in response");
        return 0;
    }

    Serial.printf("[TTS] Found marker at position: %d\n", markerPos);

    // Find the opening quote of the value (after the colon)
    int valueStart = response.indexOf('"', markerPos + 15);
    if (valueStart < 0) {
        setError("No opening quote for audioContent");
        return 0;
    }
    valueStart++;  // Skip the opening quote

    Serial.printf("[TTS] Base64 value starts at position: %d\n", valueStart);

    // Find the closing quote by searching backwards from end
    int valueEnd = -1;
    for (int i = response.length() - 1; i > valueStart; i--) {
        if (response.charAt(i) == '"') {
            valueEnd = i;
            break;
        }
    }

    if (valueEnd <= valueStart) {
        setError("No closing quote for audioContent");
        return 0;
    }

    size_t base64Len = valueEnd - valueStart;
    Serial.printf("[TTS] Raw base64 length: %d bytes\n", base64Len);

    // Extract base64 substring
    String base64Data = response.substring(valueStart, valueEnd);

    // Show first and last 30 chars
    Serial.println("[TTS] Base64 first 30 chars:");
    Serial.println(base64Data.substring(0, 30));

    Serial.println("[TTS] Base64 last 30 chars:");
    if (base64Len > 30) {
        Serial.println(base64Data.substring(base64Len - 30));
    }

    Serial.println("[TTS] Base64 extraction: OK");

    // Free the response string to save memory before decoding
    response = "";

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] STEP 6: DECODE BASE64");
    Serial.println("[TTS] ====================================================");

    // Allocate decode buffer
    size_t maxDecodeSize = maxSamples * sizeof(int16_t) + 100;
    Serial.printf("[TTS] Allocating %d bytes for decode buffer...\n", maxDecodeSize);

    uint8_t* decodeBuffer = nullptr;
    if (psramFound()) {
        decodeBuffer = (uint8_t*)ps_malloc(maxDecodeSize);
    } else {
        decodeBuffer = (uint8_t*)malloc(maxDecodeSize);
    }

    if (!decodeBuffer) {
        setError("Failed to allocate decode buffer");
        return 0;
    }

    Serial.println("[TTS] Decode buffer allocated: OK");

    // Decode base64 using robust decoder
    size_t decodedBytes = base64Decode(base64Data.c_str(), base64Data.length(), decodeBuffer, maxDecodeSize);

    // Free base64 string to save memory
    base64Data = "";

    if (decodedBytes == 0) {
        free(decodeBuffer);
        setError("Failed to decode base64");
        return 0;
    }

    Serial.printf("[TTS] Decoded %d bytes\n", decodedBytes);
    Serial.println("[TTS] Base64 decode: OK");

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] STEP 7: PARSE WAV HEADER");
    Serial.println("[TTS] ====================================================");

    // Check for WAV header
    size_t pcmOffset = 0;
    size_t pcmBytes = decodedBytes;

    if (decodedBytes > 44 &&
        decodeBuffer[0] == 'R' && decodeBuffer[1] == 'I' &&
        decodeBuffer[2] == 'F' && decodeBuffer[3] == 'F') {

        Serial.println("[TTS] WAV header detected");

        // Parse WAV header
        uint32_t chunkSize = *(uint32_t*)(decodeBuffer + 4);
        uint16_t audioFormat = *(uint16_t*)(decodeBuffer + 20);
        uint16_t numChannels = *(uint16_t*)(decodeBuffer + 22);
        uint32_t wavSampleRate = *(uint32_t*)(decodeBuffer + 24);
        uint16_t bitsPerSample = *(uint16_t*)(decodeBuffer + 34);
        uint32_t dataSize = *(uint32_t*)(decodeBuffer + 40);

        Serial.printf("[TTS] WAV chunk size: %d\n", chunkSize);
        Serial.printf("[TTS] Audio format: %d (1=PCM)\n", audioFormat);
        Serial.printf("[TTS] Channels: %d\n", numChannels);
        Serial.printf("[TTS] Sample rate: %d Hz\n", wavSampleRate);
        Serial.printf("[TTS] Bits per sample: %d\n", bitsPerSample);
        Serial.printf("[TTS] Data size: %d bytes\n", dataSize);

        pcmOffset = 44;
        pcmBytes = decodedBytes - 44;

        Serial.println("[TTS] WAV header parse: OK");
    } else {
        Serial.println("[TTS] No WAV header - assuming raw PCM");
    }

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] STEP 8: COPY PCM DATA");
    Serial.println("[TTS] ====================================================");

    size_t samples = pcmBytes / sizeof(int16_t);
    Serial.printf("[TTS] PCM bytes: %d\n", pcmBytes);
    Serial.printf("[TTS] Samples: %d\n", samples);
    Serial.printf("[TTS] Duration: %.2f seconds\n", (float)samples / sampleRate);

    if (samples > maxSamples) {
        Serial.printf("[TTS] WARNING: Truncating from %d to %d samples\n", samples, maxSamples);
        samples = maxSamples;
    }

    // Copy PCM data to output buffer
    memcpy(outputBuffer, decodeBuffer + pcmOffset, samples * sizeof(int16_t));
    free(decodeBuffer);

    // Show first and last few samples
    Serial.printf("[TTS] First 4 samples: %d %d %d %d\n",
                  outputBuffer[0], outputBuffer[1], outputBuffer[2], outputBuffer[3]);
    if (samples > 4) {
        Serial.printf("[TTS] Last 4 samples: %d %d %d %d\n",
                      outputBuffer[samples-4], outputBuffer[samples-3],
                      outputBuffer[samples-2], outputBuffer[samples-1]);
    }

    Serial.println("[TTS] PCM copy: OK");

    Serial.println("\n[TTS] ====================================================");
    Serial.println("[TTS] SYNTHESIS COMPLETE");
    Serial.printf("[TTS] Output: %d samples (%.2f seconds)\n", samples, (float)samples / sampleRate);
    Serial.println("[TTS] ====================================================\n");

    return samples;
}
