#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <functional>

class WebInterface {
public:
    using ChatCallback = std::function<String(const String& message)>;
    using VolumeCallback = std::function<void(int volume)>;

    WebInterface();

    void begin();
    void setChatCallback(ChatCallback callback);
    void setVolumeCallback(VolumeCallback callback);

    // Send updates to connected clients
    void sendStatus(const String& status);
    void sendMessage(const String& role, const String& message);

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;

    ChatCallback _chatCallback;
    VolumeCallback _volumeCallback;

    void setupRoutes();
    void handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                             AwsEventType type, void* arg, uint8_t* data, size_t len);

    // Embedded HTML/CSS/JS
    static const char* getIndexHtml();
};

#endif // WEB_SERVER_H
