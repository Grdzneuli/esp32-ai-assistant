#include "gemini_client.h"
#include "config.h"

GeminiClient::GeminiClient()
    : _apiKey(nullptr)
    , _model(GEMINI_MODEL)
    , _maxTokens(GEMINI_MAX_TOKENS)
    , _systemPrompt("")
    , _hasError(false)
    , _lastError("")
{
}

void GeminiClient::begin(const char* apiKey) {
    _apiKey = apiKey;
    _client.setInsecure();  // Skip certificate verification for simplicity
}

void GeminiClient::setModel(const char* model) {
    _model = model;
}

void GeminiClient::setMaxTokens(int maxTokens) {
    _maxTokens = maxTokens;
}

void GeminiClient::setSystemPrompt(const String& prompt) {
    _systemPrompt = prompt;
}

void GeminiClient::clearHistory() {
    _history.clear();
}

String GeminiClient::chat(const String& userMessage) {
    clearError();

    if (_apiKey == nullptr || strlen(_apiKey) == 0) {
        setError("API key not set");
        return "";
    }

    // Build the API URL
    String url = "https://";
    url += GEMINI_API_HOST;
    url += "/v1beta/models/";
    url += _model;
    url += ":generateContent?key=";
    url += _apiKey;

    // Build request body
    String requestBody = buildRequestBody(userMessage);

    Serial.println("[Gemini] Sending request...");
    Serial.println("[Gemini] URL: " + url);

    HTTPClient http;
    http.begin(_client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(30000);  // 30 second timeout

    int httpCode = http.POST(requestBody);

    String response = "";

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            response = parseResponse(payload);

            if (!_hasError && response.length() > 0) {
                // Add to conversation history
                _history.push_back({"user", userMessage});
                _history.push_back({"model", response});

                // Limit history size
                while (_history.size() > MAX_CONVERSATION_HISTORY * 2) {
                    _history.erase(_history.begin());
                    _history.erase(_history.begin());
                }
            }
        } else {
            String errorBody = http.getString();
            setError("HTTP error " + String(httpCode) + ": " + errorBody);
        }
    } else {
        setError("Connection failed: " + http.errorToString(httpCode));
    }

    http.end();
    return response;
}

String GeminiClient::buildRequestBody(const String& userMessage) {
    JsonDocument doc;

    // Build contents array with conversation history
    JsonArray contents = doc["contents"].to<JsonArray>();

    // Add system prompt as first user message if set
    if (_systemPrompt.length() > 0 && _history.empty()) {
        JsonObject systemMsg = contents.add<JsonObject>();
        systemMsg["role"] = "user";
        JsonArray parts = systemMsg["parts"].to<JsonArray>();
        JsonObject part = parts.add<JsonObject>();
        part["text"] = "System instructions: " + _systemPrompt;

        // Add model acknowledgment
        JsonObject ackMsg = contents.add<JsonObject>();
        ackMsg["role"] = "model";
        JsonArray ackParts = ackMsg["parts"].to<JsonArray>();
        JsonObject ackPart = ackParts.add<JsonObject>();
        ackPart["text"] = "Understood. I will follow these instructions.";
    }

    // Add conversation history
    for (const auto& msg : _history) {
        JsonObject historyMsg = contents.add<JsonObject>();
        historyMsg["role"] = msg.role;
        JsonArray parts = historyMsg["parts"].to<JsonArray>();
        JsonObject part = parts.add<JsonObject>();
        part["text"] = msg.content;
    }

    // Add current user message
    JsonObject userMsg = contents.add<JsonObject>();
    userMsg["role"] = "user";
    JsonArray parts = userMsg["parts"].to<JsonArray>();
    JsonObject part = parts.add<JsonObject>();
    part["text"] = userMessage;

    // Generation config
    JsonObject genConfig = doc["generationConfig"].to<JsonObject>();
    genConfig["maxOutputTokens"] = _maxTokens;
    genConfig["temperature"] = 0.7;

    // Safety settings (set to minimum blocking)
    JsonArray safetySettings = doc["safetySettings"].to<JsonArray>();
    const char* categories[] = {
        "HARM_CATEGORY_HARASSMENT",
        "HARM_CATEGORY_HATE_SPEECH",
        "HARM_CATEGORY_SEXUALLY_EXPLICIT",
        "HARM_CATEGORY_DANGEROUS_CONTENT"
    };
    for (const char* category : categories) {
        JsonObject setting = safetySettings.add<JsonObject>();
        setting["category"] = category;
        setting["threshold"] = "BLOCK_NONE";
    }

    String output;
    serializeJson(doc, output);
    return output;
}

String GeminiClient::parseResponse(const String& response) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        setError("JSON parse error: " + String(error.c_str()));
        return "";
    }

    // Check for API error
    if (doc.containsKey("error")) {
        String errorMsg = doc["error"]["message"].as<String>();
        setError("API error: " + errorMsg);
        return "";
    }

    // Extract text from response
    if (doc.containsKey("candidates") && doc["candidates"].size() > 0) {
        JsonObject candidate = doc["candidates"][0];

        if (candidate.containsKey("content")) {
            JsonArray parts = candidate["content"]["parts"];
            if (parts.size() > 0) {
                String text = parts[0]["text"].as<String>();
                Serial.println("[Gemini] Response received: " + String(text.length()) + " chars");
                return text;
            }
        }

        // Check for blocked content
        if (candidate.containsKey("finishReason")) {
            String reason = candidate["finishReason"].as<String>();
            if (reason == "SAFETY") {
                setError("Response blocked by safety filter");
                return "";
            }
        }
    }

    setError("No response content found");
    return "";
}

void GeminiClient::setError(const String& error) {
    _hasError = true;
    _lastError = error;
    Serial.println("[Gemini] Error: " + error);
}

void GeminiClient::clearError() {
    _hasError = false;
    _lastError = "";
}
