#include "wifi_manager.h"
#include "config.h"

WiFiManager::WiFiManager()
    : _ssid(nullptr)
    , _password(nullptr)
    , _state(State::DISCONNECTED)
    , _statusCallback(nullptr)
    , _lastCheckTime(0)
{
}

void WiFiManager::begin(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
}

void WiFiManager::setStatusCallback(StatusCallback callback) {
    _statusCallback = callback;
}

bool WiFiManager::connect(uint32_t timeoutMs) {
    if (_ssid == nullptr || strlen(_ssid) == 0) {
        setState(State::ERROR, "No SSID configured");
        return false;
    }

    setState(State::CONNECTING, String("Connecting to ") + _ssid);

    WiFi.begin(_ssid, _password);

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            setState(State::ERROR, "Connection timeout");
            return false;
        }
        delay(100);
    }

    setState(State::CONNECTED, "Connected: " + getIP());
    return true;
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    setState(State::DISCONNECTED, "Disconnected");
}

bool WiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIP() const {
    return WiFi.localIP().toString();
}

String WiFiManager::getSSID() const {
    return WiFi.SSID();
}

int8_t WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

void WiFiManager::update() {
    // Check connection status periodically
    if (millis() - _lastCheckTime > 5000) {
        _lastCheckTime = millis();

        bool connected = isConnected();

        if (connected && _state != State::CONNECTED) {
            setState(State::CONNECTED, "Reconnected: " + getIP());
        } else if (!connected && _state == State::CONNECTED) {
            setState(State::DISCONNECTED, "Connection lost");
        }
    }
}

void WiFiManager::setState(State state, const String& message) {
    _state = state;
    if (_statusCallback) {
        _statusCallback(state, message);
    }

    // Also log to serial
    Serial.print("[WiFi] ");
    switch (state) {
        case State::DISCONNECTED: Serial.print("DISCONNECTED: "); break;
        case State::CONNECTING:   Serial.print("CONNECTING: "); break;
        case State::CONNECTED:    Serial.print("CONNECTED: "); break;
        case State::ERROR:        Serial.print("ERROR: "); break;
    }
    Serial.println(message);
}
