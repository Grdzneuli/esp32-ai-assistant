#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <Arduino.h>
#include <driver/i2s.h>

class AudioOutput {
public:
    AudioOutput();
    ~AudioOutput();

    bool begin();
    void end();

    // Volume control (0-100)
    void setVolume(int volume);
    int getVolume() const { return _volume; }

    // Play audio buffer
    void play(const int16_t* samples, size_t count);
    void playTone(int frequency, int durationMs);

    // Feedback sounds
    void playBeep();          // Short beep for button press
    void playStartSound();    // Sound when starting to listen
    void playStopSound();     // Sound when stopping
    void playErrorSound();    // Error indication

    // State
    bool isPlaying() const { return _playing; }
    void stop();

private:
    bool _initialized;
    bool _playing;
    int _volume;

    bool configureI2S();
    void applyVolume(int16_t* samples, size_t count);
};

#endif // AUDIO_OUTPUT_H
