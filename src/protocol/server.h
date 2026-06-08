#pragma once
#include <string>
#include <functional>
#include <cstdint>

// ──────────────────────────────────────────
//  Minimal HTTP + WebSocket server
//
//  Serves a single HTML file via HTTP GET /
//  Upgrades the connection to WebSocket for
//  the game protocol.
//
//  Single-threaded. One client at a time.
//  No external dependencies.
// ──────────────────────────────────────────
class WebSocketServer {
public:
    // Called when a text message arrives from the browser
    using MessageHandler = std::function<void(const std::string&)>;

    WebSocketServer();
    ~WebSocketServer();

    // Start listening. Blocks until a client connects,
    // then calls onMessage for each message received.
    // Returns false if binding fails.
    bool listen(int port, const std::string& htmlFile,
                MessageHandler onMessage);

    // Send a JSON message to the connected browser
    void send(const std::string& json);

    // Close the connection
    void close();

private:
    int serverFd = -1;
    int clientFd = -1;

    bool doHandshake(const std::string& htmlContent);
    std::string readHttpRequest();
    bool isWebSocketUpgrade(const std::string& request);
    std::string extractWebSocketKey(const std::string& request);

    // WebSocket framing
    bool sendFrame(const std::string& data);
    bool recvFrame(std::string& data);

    // SHA-1 and Base64 for WebSocket handshake
    void  sha1(const uint8_t* data, size_t len, uint8_t digest[20]);
    std::string base64Encode(const uint8_t* data, size_t len);
};
