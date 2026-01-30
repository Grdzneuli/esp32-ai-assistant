#include "web_server.h"
#include "config.h"
#include <ArduinoJson.h>

WebInterface::WebInterface()
    : _server(WEB_SERVER_PORT)
    , _ws("/ws")
    , _chatCallback(nullptr)
    , _volumeCallback(nullptr)
{
}

void WebInterface::begin() {
    setupRoutes();

    _ws.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                       AwsEventType type, void* arg, uint8_t* data, size_t len) {
        handleWebSocketEvent(server, client, type, arg, data, len);
    });

    _server.addHandler(&_ws);
    _server.begin();

    Serial.printf("[WebServer] Started on port %d\n", WEB_SERVER_PORT);
}

void WebInterface::setChatCallback(ChatCallback callback) {
    _chatCallback = callback;
}

void WebInterface::setVolumeCallback(VolumeCallback callback) {
    _volumeCallback = callback;
}

void WebInterface::sendStatus(const String& status) {
    JsonDocument doc;
    doc["type"] = "status";
    doc["status"] = status;

    String json;
    serializeJson(doc, json);
    _ws.textAll(json);
}

void WebInterface::sendMessage(const String& role, const String& message) {
    JsonDocument doc;
    doc["type"] = "message";
    doc["role"] = role;
    doc["content"] = message;

    String json;
    serializeJson(doc, json);
    _ws.textAll(json);
}

void WebInterface::setupRoutes() {
    // Main page
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", getIndexHtml());
    });

    // API endpoint for chat (fallback for non-WebSocket)
    _server.on("/api/chat", HTTP_POST, [](AsyncWebServerRequest* request) {},
        NULL,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len,
               size_t index, size_t total) {
            String body = String((char*)data).substring(0, len);

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);

            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }

            String message = doc["message"].as<String>();

            if (_chatCallback) {
                String response = _chatCallback(message);

                JsonDocument respDoc;
                respDoc["response"] = response;

                String respJson;
                serializeJson(respDoc, respJson);
                request->send(200, "application/json", respJson);
            } else {
                request->send(503, "application/json", "{\"error\":\"Service unavailable\"}");
            }
        });

    // Status endpoint
    _server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["rssi"] = WiFi.RSSI();

        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });
}

void WebInterface::handleWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                        AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("[WebSocket] Client #%u connected from %s\n",
                client->id(), client->remoteIP().toString().c_str());
            sendStatus("connected");
            break;

        case WS_EVT_DISCONNECT:
            Serial.printf("[WebSocket] Client #%u disconnected\n", client->id());
            break;

        case WS_EVT_DATA: {
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                String message = String((char*)data).substring(0, len);

                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, message);

                if (!error) {
                    String msgType = doc["type"].as<String>();

                    if (msgType == "chat" && _chatCallback) {
                        String userMsg = doc["message"].as<String>();
                        sendMessage("user", userMsg);

                        String response = _chatCallback(userMsg);
                        sendMessage("assistant", response);

                    } else if (msgType == "volume" && _volumeCallback) {
                        int volume = doc["value"].as<int>();
                        _volumeCallback(volume);
                    }
                }
            }
            break;
        }

        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

const char* WebInterface::getIndexHtml() {
    return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32-S3 AI Assistant</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            color: #fff;
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
        }

        header {
            text-align: center;
            padding: 20px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
            margin-bottom: 20px;
        }

        header h1 {
            font-size: 1.8em;
            background: linear-gradient(90deg, #00d9ff, #00ff88);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        .status {
            display: flex;
            justify-content: center;
            gap: 20px;
            margin-top: 10px;
            font-size: 0.9em;
            color: #888;
        }

        .status-dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            display: inline-block;
            margin-right: 5px;
        }

        .status-dot.connected { background: #00ff88; }
        .status-dot.disconnected { background: #ff4444; }

        .chat-container {
            background: rgba(255,255,255,0.05);
            border-radius: 15px;
            height: 50vh;
            overflow-y: auto;
            padding: 20px;
            margin-bottom: 20px;
        }

        .message {
            margin-bottom: 15px;
            animation: fadeIn 0.3s ease;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(10px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .message.user {
            text-align: right;
        }

        .message .bubble {
            display: inline-block;
            max-width: 80%;
            padding: 12px 18px;
            border-radius: 18px;
            line-height: 1.4;
        }

        .message.user .bubble {
            background: linear-gradient(135deg, #00d9ff, #0099cc);
            border-bottom-right-radius: 4px;
        }

        .message.assistant .bubble {
            background: rgba(255,255,255,0.1);
            border-bottom-left-radius: 4px;
        }

        .message .label {
            font-size: 0.75em;
            color: #666;
            margin-bottom: 4px;
        }

        .input-area {
            display: flex;
            gap: 10px;
        }

        #messageInput {
            flex: 1;
            padding: 15px 20px;
            border: none;
            border-radius: 25px;
            background: rgba(255,255,255,0.1);
            color: #fff;
            font-size: 1em;
            outline: none;
            transition: background 0.3s;
        }

        #messageInput:focus {
            background: rgba(255,255,255,0.15);
        }

        #messageInput::placeholder {
            color: #666;
        }

        button {
            padding: 15px 30px;
            border: none;
            border-radius: 25px;
            background: linear-gradient(135deg, #00d9ff, #00ff88);
            color: #1a1a2e;
            font-weight: bold;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }

        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(0, 217, 255, 0.3);
        }

        button:active {
            transform: translateY(0);
        }

        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
            transform: none;
        }

        .controls {
            display: flex;
            justify-content: center;
            gap: 20px;
            margin-top: 20px;
        }

        .volume-control {
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .volume-control input[type="range"] {
            width: 100px;
        }

        .typing-indicator {
            display: none;
            padding: 10px;
        }

        .typing-indicator.active {
            display: block;
        }

        .typing-indicator .dots {
            display: inline-flex;
            gap: 4px;
        }

        .typing-indicator .dot {
            width: 8px;
            height: 8px;
            background: #00d9ff;
            border-radius: 50%;
            animation: bounce 1.4s infinite ease-in-out;
        }

        .typing-indicator .dot:nth-child(1) { animation-delay: -0.32s; }
        .typing-indicator .dot:nth-child(2) { animation-delay: -0.16s; }

        @keyframes bounce {
            0%, 80%, 100% { transform: scale(0); }
            40% { transform: scale(1); }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>ESP32-S3 AI Assistant</h1>
            <div class="status">
                <span><span class="status-dot disconnected" id="wsStatus"></span>WebSocket</span>
                <span id="rssi">RSSI: --</span>
            </div>
        </header>

        <div class="chat-container" id="chatContainer">
            <div class="message assistant">
                <div class="label">Assistant</div>
                <div class="bubble">Hello! I'm your AI assistant. How can I help you today?</div>
            </div>
        </div>

        <div class="typing-indicator" id="typingIndicator">
            <div class="dots">
                <span class="dot"></span>
                <span class="dot"></span>
                <span class="dot"></span>
            </div>
        </div>

        <div class="input-area">
            <input type="text" id="messageInput" placeholder="Type your message..." autocomplete="off">
            <button id="sendBtn">Send</button>
        </div>

        <div class="controls">
            <div class="volume-control">
                <span>Volume:</span>
                <input type="range" id="volumeSlider" min="0" max="100" value="70">
                <span id="volumeValue">70%</span>
            </div>
        </div>
    </div>

    <script>
        const chatContainer = document.getElementById('chatContainer');
        const messageInput = document.getElementById('messageInput');
        const sendBtn = document.getElementById('sendBtn');
        const wsStatus = document.getElementById('wsStatus');
        const typingIndicator = document.getElementById('typingIndicator');
        const volumeSlider = document.getElementById('volumeSlider');
        const volumeValue = document.getElementById('volumeValue');

        let ws = null;

        function connectWebSocket() {
            ws = new WebSocket(`ws://${location.host}/ws`);

            ws.onopen = () => {
                wsStatus.className = 'status-dot connected';
                console.log('WebSocket connected');
            };

            ws.onclose = () => {
                wsStatus.className = 'status-dot disconnected';
                console.log('WebSocket disconnected');
                setTimeout(connectWebSocket, 3000);
            };

            ws.onmessage = (event) => {
                const data = JSON.parse(event.data);

                if (data.type === 'message') {
                    addMessage(data.role, data.content);
                    typingIndicator.classList.remove('active');
                } else if (data.type === 'status') {
                    console.log('Status:', data.status);
                }
            };
        }

        function addMessage(role, content) {
            const msgDiv = document.createElement('div');
            msgDiv.className = `message ${role}`;
            msgDiv.innerHTML = `
                <div class="label">${role === 'user' ? 'You' : 'Assistant'}</div>
                <div class="bubble">${escapeHtml(content)}</div>
            `;
            chatContainer.appendChild(msgDiv);
            chatContainer.scrollTop = chatContainer.scrollHeight;
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        function sendMessage() {
            const message = messageInput.value.trim();
            if (!message) return;

            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'chat', message }));
                messageInput.value = '';
                typingIndicator.classList.add('active');
            } else {
                alert('Not connected to server');
            }
        }

        sendBtn.addEventListener('click', sendMessage);
        messageInput.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') sendMessage();
        });

        volumeSlider.addEventListener('input', () => {
            const value = volumeSlider.value;
            volumeValue.textContent = value + '%';

            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'volume', value: parseInt(value) }));
            }
        });

        // Fetch status periodically
        setInterval(async () => {
            try {
                const resp = await fetch('/api/status');
                const data = await resp.json();
                document.getElementById('rssi').textContent = `RSSI: ${data.rssi} dBm`;
            } catch (e) {}
        }, 5000);

        connectWebSocket();
    </script>
</body>
</html>
)rawliteral";
}
