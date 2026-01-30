#ifndef GEMINI_CLIENT_H
#define GEMINI_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>

struct ChatMessage {
    String role;    // "user" or "model"
    String content;
};

class GeminiClient {
public:
    GeminiClient();

    void begin(const char* apiKey);
    void setModel(const char* model);
    void setMaxTokens(int maxTokens);
    void setSystemPrompt(const String& prompt);

    // Send a message and get response
    String chat(const String& userMessage);

    // Clear conversation history
    void clearHistory();

    // Get conversation history
    const std::vector<ChatMessage>& getHistory() const { return _history; }

    // Error handling
    bool hasError() const { return _hasError; }
    String getLastError() const { return _lastError; }

private:
    const char* _apiKey;
    String _model;
    int _maxTokens;
    String _systemPrompt;
    std::vector<ChatMessage> _history;

    bool _hasError;
    String _lastError;

    WiFiClientSecure _client;

    String buildRequestBody(const String& userMessage);
    String parseResponse(const String& response);
    void setError(const String& error);
    void clearError();
};

#endif // GEMINI_CLIENT_H
