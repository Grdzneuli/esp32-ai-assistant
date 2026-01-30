#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <functional>

class WiFiManager {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR
    };

    using StatusCallback = std::function<void(State state, const String& message)>;

    WiFiManager();

    void begin(const char* ssid, const char* password);
    void setStatusCallback(StatusCallback callback);

    bool connect(uint32_t timeoutMs = 10000);
    void disconnect();
    bool isConnected();

    State getState() const { return _state; }
    String getIP() const;
    String getSSID() const;
    int8_t getRSSI() const;

    void update();  // Call in loop for connection monitoring

private:
    const char* _ssid;
    const char* _password;
    State _state;
    StatusCallback _statusCallback;
    uint32_t _lastCheckTime;

    void setState(State state, const String& message = "");
};

#endif // WIFI_MANAGER_H
