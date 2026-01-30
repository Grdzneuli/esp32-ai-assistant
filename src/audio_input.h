#ifndef AUDIO_INPUT_H
#define AUDIO_INPUT_H

#include <Arduino.h>
#include <driver/i2s.h>
#include <functional>

class AudioInput {
public:
    using AudioCallback = std::function<void(int16_t* samples, size_t count)>;

    AudioInput();
    ~AudioInput();

    bool begin();
    void end();

    // Recording control
    void startRecording();
    void stopRecording();
    bool isRecording() const { return _recording; }

    // Get audio buffer (call after stopRecording)
    int16_t* getBuffer() const { return _buffer; }
    size_t getBufferSize() const { return _bufferPos; }

    // Clear buffer
    void clearBuffer();

    // Voice Activity Detection
    bool detectVoice();  // Returns true if voice detected
    int getAverageLevel();  // Get current audio level

    // Callback for real-time audio processing
    void setAudioCallback(AudioCallback callback) { _callback = callback; }

    // Process audio (call in loop when recording)
    void process();

private:
    bool _initialized;
    bool _recording;

    int16_t* _buffer;
    size_t _bufferSize;
    size_t _bufferPos;

    int16_t* _readBuffer;
    size_t _readBufferSize;

    AudioCallback _callback;

    // VAD
    uint32_t _lastSoundTime;
    int _avgLevel;

    bool configureI2S();
};

#endif // AUDIO_INPUT_H
