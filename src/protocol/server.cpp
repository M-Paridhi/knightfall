#include "server.h"
#include <cstring>
#include <cstdio>
#include <string>
#include <sstream>

// ── Platform socket headers ───────────────
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
  static inline void sockClose(int fd) { closesocket((SOCKET)fd); }
  static inline void sockInit() {
      WSADATA w; WSAStartup(MAKEWORD(2,2), &w);
  }
  static inline int sockRecv(int fd, char* buf, int len) {
      return recv((SOCKET)fd, buf, len, 0);
  }
  static inline int sockSend(int fd, const char* buf, int len) {
      return send((SOCKET)fd, buf, len, 0);
  }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  static inline void sockClose(int fd) { ::close(fd); }
  static inline void sockInit()        { }
  static inline int sockRecv(int fd, char* buf, int len) {
      return ::recv(fd, buf, len, 0);
  }
  static inline int sockSend(int fd, const char* buf, int len) {
      return ::send(fd, buf, len, 0);
  }
#endif

// ══════════════════════════════════════════
//  SHA-1 — needed for WebSocket handshake
//  (RFC 3174 / RFC 6455)
// ══════════════════════════════════════════
void WebSocketServer::sha1(const uint8_t* data, size_t len, uint8_t digest[20]) {
    uint32_t h0=0x67452301, h1=0xEFCDAB89,
             h2=0x98BADCFE, h3=0x10325476, h4=0xC3D2E1F0;

    auto rol = [](uint32_t v, int n) { return (v<<n)|(v>>(32-n)); };

    // Pre-processing: adding padding bits
    size_t msgLen = len;
    size_t totalLen = ((len + 8) / 64 + 1) * 64;
    uint8_t* msg = new uint8_t[totalLen]();
    memcpy(msg, data, len);
    msg[len] = 0x80;
    // Append original length in bits as 64-bit big-endian
    uint64_t bitLen = (uint64_t)msgLen * 8;
    for (int i = 0; i < 8; ++i)
        msg[totalLen - 8 + i] = (uint8_t)(bitLen >> (56 - i*8));

    for (size_t i = 0; i < totalLen; i += 64) {
        uint32_t w[80];
        for (int j = 0; j < 16; ++j)
            w[j] = ((uint32_t)msg[i+j*4]   << 24) | ((uint32_t)msg[i+j*4+1] << 16)
                 | ((uint32_t)msg[i+j*4+2] <<  8) |  (uint32_t)msg[i+j*4+3];
        for (int j = 16; j < 80; ++j) {
            uint32_t t = w[j-3]^w[j-8]^w[j-14]^w[j-16];
            w[j] = rol(t, 1);
        }
        uint32_t a=h0, b=h1, c=h2, d=h3, e=h4;
        for (int j = 0; j < 80; ++j) {
            uint32_t f, k;
            if      (j < 20) { f=(b&c)|((~b)&d); k=0x5A827999; }
            else if (j < 40) { f=b^c^d;           k=0x6ED9EBA1; }
            else if (j < 60) { f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC; }
            else              { f=b^c^d;           k=0xCA62C1D6; }
            uint32_t t = rol(a,5) + f + e + k + w[j];
            e=d; d=c; c=rol(b,30); b=a; a=t;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }
    delete[] msg;

    uint32_t hh[5] = {h0,h1,h2,h3,h4};
    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 4; ++j)
            digest[i*4+j] = (uint8_t)(hh[i] >> (24-j*8));
}

// ══════════════════════════════════════════
//  Base64 encode
// ══════════════════════════════════════════
std::string WebSocketServer::base64Encode(const uint8_t* data, size_t len) {
    static const char* t =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t b = (uint32_t)data[i] << 16;
        if (i+1 < len) b |= (uint32_t)data[i+1] << 8;
        if (i+2 < len) b |= (uint32_t)data[i+2];
        out += t[(b>>18)&63];
        out += t[(b>>12)&63];
        out += (i+1 < len) ? t[(b>>6)&63] : '=';
        out += (i+2 < len) ? t[(b   )&63] : '=';
    }
    return out;
}

// ══════════════════════════════════════════
//  Constructor / Destructor
// ══════════════════════════════════════════
WebSocketServer::WebSocketServer()  { sockInit(); }
WebSocketServer::~WebSocketServer() {
    if (clientFd >= 0) sockClose(clientFd);
    if (serverFd >= 0) sockClose(serverFd);
}

void WebSocketServer::close() {
    if (clientFd >= 0) { sockClose(clientFd); clientFd = -1; }
}

// ══════════════════════════════════════════
//  HTTP request parsing
// ══════════════════════════════════════════
std::string WebSocketServer::readHttpRequest() {
    std::string req;
    char buf[4096]; int n;
    while ((n = sockRecv(clientFd, buf, sizeof(buf)-1)) > 0) {
        buf[n] = '\0';
        req += buf;
        if (req.find("\r\n\r\n") != std::string::npos) break;
    }
    return req;
}

bool WebSocketServer::isWebSocketUpgrade(const std::string& req) {
    return req.find("Upgrade: websocket") != std::string::npos ||
           req.find("Upgrade: WebSocket") != std::string::npos;
}

std::string WebSocketServer::extractWebSocketKey(const std::string& req) {
    const std::string marker = "Sec-WebSocket-Key: ";
    size_t pos = req.find(marker);
    if (pos == std::string::npos) return "";
    pos += marker.size();
    size_t end = req.find("\r\n", pos);
    return req.substr(pos, end - pos);
}

// ══════════════════════════════════════════
//  WebSocket framing
//  Server → Client: no masking (FIN + text opcode)
//  Client → Server: 4-byte mask
// ══════════════════════════════════════════
bool WebSocketServer::sendFrame(const std::string& data) {
    std::string frame;
    frame += (char)0x81;   // FIN bit + text opcode

    size_t len = data.size();
    if (len < 126) {
        frame += (char)len;
    } else if (len < 65536) {
        frame += (char)126;
        frame += (char)(len >> 8);
        frame += (char)(len & 0xFF);
    } else {
        return false;  // we never send huge frames
    }
    frame += data;

    int sent = sockSend(clientFd, frame.data(), (int)frame.size());
    return sent == (int)frame.size();
}

bool WebSocketServer::recvFrame(std::string& data) {
    data.clear();
    uint8_t header[2];
    if (sockRecv(clientFd, (char*)header, 2) != 2) return false;

    // uint8_t fin  = (header[0] >> 7) & 1;
    uint8_t op   = header[0] & 0x0F;
    bool    mask = (header[1] >> 7) & 1;
    uint64_t plen = header[1] & 0x7F;

    if (op == 0x8) return false;   // close frame

    if (plen == 126) {
        uint8_t ext[2];
        if (sockRecv(clientFd, (char*)ext, 2) != 2) return false;
        plen = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (plen == 127) {
        uint8_t ext[8];
        if (sockRecv(clientFd, (char*)ext, 8) != 8) return false;
        plen = 0;
        for (int i = 0; i < 8; ++i) plen = (plen<<8)|ext[i];
    }

    uint8_t maskKey[4] = {};
    if (mask) {
        if (sockRecv(clientFd, (char*)maskKey, 4) != 4) return false;
    }

    // Read payload
    std::string payload(plen, '\0');
    size_t got = 0;
    while (got < plen) {
        int n = sockRecv(clientFd, &payload[got], (int)(plen - got));
        if (n <= 0) return false;
        got += n;
    }

    // Unmask
    if (mask)
        for (size_t i = 0; i < plen; ++i)
            payload[i] ^= maskKey[i % 4];

    data = payload;
    return true;
}

// ══════════════════════════════════════════
//  Handshake: serve HTML or upgrade to WS
// ══════════════════════════════════════════
bool WebSocketServer::doHandshake(const std::string& htmlContent) {
    std::string req = readHttpRequest();
    if (req.empty()) return false;

    if (isWebSocketUpgrade(req)) {
        // WebSocket upgrade
        std::string key = extractWebSocketKey(req);
        std::string magic = key + "258EAFA5-E914-4789-ABB3-E2F5A2B1D3F7";

        uint8_t digest[20];
        sha1((const uint8_t*)magic.data(), magic.size(), digest);
        std::string accept = base64Encode(digest, 20);

        std::string response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";

        sockSend(clientFd, response.data(), (int)response.size());
        return true;

    } else {
        // HTTP request: serve the HTML file
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(htmlContent.size()) + "\r\n"
            "Connection: close\r\n\r\n" + htmlContent;

        sockSend(clientFd, response.data(), (int)response.size());
        sockClose(clientFd);
        clientFd = -1;
        return false;  // not a WS connection
    }
}

// ══════════════════════════════════════════
//  Main listen loop
// ══════════════════════════════════════════
bool WebSocketServer::listen(int port, const std::string& htmlFile,
                             MessageHandler onMessage) {
    // Read the HTML file into memory
    FILE* f = fopen(htmlFile.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "[server] Cannot open %s\n", htmlFile.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END); long flen = ftell(f); fseek(f, 0, SEEK_SET);
    std::string html(flen, '\0');
    fread(&html[0], 1, flen, f);
    fclose(f);

    // Create server socket
#ifdef _WIN32
    serverFd = (int)socket(AF_INET, SOCK_STREAM, 0);
#else
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
#endif
    if (serverFd < 0) { perror("socket"); return false; }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (::bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return false;
    }
    ::listen(serverFd, 5);

    printf("[Knightfall] Open your browser at http://localhost:%d\n", port);
    fflush(stdout);

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        clientFd = (int)accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) continue;

        bool isWS = doHandshake(html);
        if (!isWS) continue;

        // WebSocket connection established — message loop
        std::string msg;
        while (recvFrame(msg)) {
            if (onMessage) onMessage(msg);
        }

        sockClose(clientFd);
        clientFd = -1;
    }
    return true;
}

void WebSocketServer::send(const std::string& json) {
    if (clientFd >= 0) sendFrame(json);
}
