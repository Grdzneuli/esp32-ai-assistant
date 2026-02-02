#ifndef SPEECH_CLIENT_H
#define SPEECH_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class SpeechClient {
public:
    SpeechClient();

    void begin(const char* apiKey);

    // Speech-to-Text: Convert audio buffer to text
    // audioBuffer: 16-bit PCM samples
    // sampleCount: number of samples
    // sampleRate: sample rate in Hz (e.g., 16000)
    String transcribe(const int16_t* audioBuffer, size_t sampleCount, int sampleRate = 16000);

    // Text-to-Speech: Convert text to audio
    // Returns number of samples written to outputBuffer
    // outputBuffer should be pre-allocated (use getEstimatedSamples to estimate size)
    size_t synthesize(const String& text, int16_t* outputBuffer, size_t maxSamples, int sampleRate = 16000);

    // Estimate output buffer size needed for TTS
    size_t getEstimatedSamples(const String& text, int sampleRate = 16000);

    // Error handling
    bool hasError() const { return _hasError; }
    String getLastError() const { return _lastError; }

    // Configuration
    void setLanguage(const String& languageCode) { _languageCode = languageCode; }
    void setVoice(const String& voiceName) { _voiceName = voiceName; }

private:
    String _apiKey;
    String _languageCode;
    String _voiceName;
    bool _hasError;
    String _lastError;

    String base64Encode(const uint8_t* data, size_t length);
    size_t base64Decode(const String& input, uint8_t* output, size_t maxLength);
    size_t base64Decode(const char* input, size_t inputLen, uint8_t* output, size_t maxLength);

    void setError(const String& error);
    void clearError();
};

#endif // SPEECH_CLIENT_H
