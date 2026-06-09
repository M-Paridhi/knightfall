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
  static inline void sockInit() { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  static inline void sockClose(int fd) { ::close(fd); }
  static inline void sockInit()        { }
#endif

// ── Reliable read/write helpers ───────────
// TCP does not guarantee recv/send transfers
// the full buffer in one call. These helpers
// loop until all bytes are transferred.

static bool readExact(int fd, uint8_t* buf, int len) {
    int got = 0;
    while (got < len) {
#ifdef _WIN32
        int n = recv((SOCKET)fd, (char*)(buf + got), len - got, 0);
#else
        int n = ::recv(fd, buf + got, len - got, 0);
#endif
        if (n <= 0) return false;
        got += n;
    }
    return true;
}

static bool sendAll(int fd, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = send((SOCKET)fd, buf + sent, len - sent, 0);
#else
        int n = ::send(fd, buf + sent, len - sent, 0);
#endif
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

// ══════════════════════════════════════════
//  SHA-1 (RFC 3174) — required for WebSocket
//  handshake per RFC 6455 Section 4.2.2
// ══════════════════════════════════════════
void WebSocketServer::sha1(const uint8_t* data, size_t len, uint8_t digest[20]) {
    uint32_t h0=0x67452301, h1=0xEFCDAB89,
             h2=0x98BADCFE, h3=0x10325476, h4=0xC3D2E1F0;

    auto rol = [](uint32_t v, int n){ return (v<<n)|(v>>(32-n)); };

    size_t totalLen = ((len + 8) / 64 + 1) * 64;
    uint8_t* msg = new uint8_t[totalLen]();
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bitLen = (uint64_t)len * 8;
    for (int i = 0; i < 8; ++i)
        msg[totalLen - 8 + i] = (uint8_t)(bitLen >> (56 - i*8));

    for (size_t i = 0; i < totalLen; i += 64) {
        uint32_t w[80];
        for (int j = 0; j < 16; ++j)
            w[j] = ((uint32_t)msg[i+j*4]   << 24)
                 | ((uint32_t)msg[i+j*4+1] << 16)
                 | ((uint32_t)msg[i+j*4+2] <<  8)
                 |  (uint32_t)msg[i+j*4+3];
        for (int j = 16; j < 80; ++j) {
            uint32_t t = w[j-3]^w[j-8]^w[j-14]^w[j-16];
            w[j] = rol(t, 1);
        }
        uint32_t a=h0,b=h1,c=h2,d=h3,e=h4;
        for (int j = 0; j < 80; ++j) {
            uint32_t f, k;
            if      (j<20){f=(b&c)|((~b)&d); k=0x5A827999;}
            else if (j<40){f=b^c^d;           k=0x6ED9EBA1;}
            else if (j<60){f=(b&c)|(b&d)|(c&d);k=0x8F1BBCDC;}
            else          {f=b^c^d;            k=0xCA62C1D6;}
            uint32_t t = rol(a,5)+f+e+k+w[j];
            e=d; d=c; c=rol(b,30); b=a; a=t;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }
    delete[] msg;

    uint32_t hh[5]={h0,h1,h2,h3,h4};
    for (int i=0;i<5;++i)
        for (int j=0;j<4;++j)
            digest[i*4+j]=(uint8_t)(hh[i]>>(24-j*8));
}

// ══════════════════════════════════════════
//  Base64
// ══════════════════════════════════════════
std::string WebSocketServer::base64Encode(const uint8_t* data, size_t len) {
    static const char* t =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i=0;i<len;i+=3){
        uint32_t b=(uint32_t)data[i]<<16;
        if(i+1<len) b|=(uint32_t)data[i+1]<<8;
        if(i+2<len) b|=(uint32_t)data[i+2];
        out+=t[(b>>18)&63]; out+=t[(b>>12)&63];
        out+=(i+1<len)?t[(b>>6)&63]:'=';
        out+=(i+2<len)?t[(b   )&63]:'=';
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
//  HTTP request reading
// ══════════════════════════════════════════
std::string WebSocketServer::readHttpRequest() {
    std::string req;
    char buf[1];
    // Read byte-by-byte to find \r\n\r\n without over-reading
    while (true) {
#ifdef _WIN32
        int n = recv((SOCKET)clientFd, buf, 1, 0);
#else
        int n = ::recv(clientFd, buf, 1, 0);
#endif
        if (n <= 0) break;
        req += buf[0];
        if (req.size() >= 4 &&
            req[req.size()-4]=='\r' && req[req.size()-3]=='\n' &&
            req[req.size()-2]=='\r' && req[req.size()-1]=='\n')
            break;
        if (req.size() > 8192) break;  // safety limit
    }
    return req;
}

bool WebSocketServer::isWebSocketUpgrade(const std::string& req) {
    // Case-insensitive search for "upgrade: websocket"
    std::string lower = req;
    for (auto& c : lower) c = (char)tolower((unsigned char)c);
    return lower.find("upgrade: websocket") != std::string::npos;
}

std::string WebSocketServer::extractWebSocketKey(const std::string& req) {
    // Try both capitalizations
    for (auto& marker : {"Sec-WebSocket-Key: ", "sec-websocket-key: "}) {
        size_t pos = req.find(marker);
        if (pos != std::string::npos) {
            pos += strlen(marker);
            size_t end = req.find("\r\n", pos);
            if (end != std::string::npos)
                return req.substr(pos, end - pos);
        }
    }
    return "";
}

// ══════════════════════════════════════════
//  WebSocket frame encoding / decoding
// ══════════════════════════════════════════
bool WebSocketServer::sendFrame(const std::string& data) {
    std::string frame;
    frame += (char)0x81;  // FIN + text opcode

    size_t len = data.size();
    if (len < 126) {
        frame += (char)len;
    } else if (len < 65536) {
        frame += (char)126;
        frame += (char)(len >> 8);
        frame += (char)(len & 0xFF);
    } else {
        return false;
    }
    frame += data;
    return sendAll(clientFd, frame.data(), (int)frame.size());
}

bool WebSocketServer::recvFrame(std::string& data) {
    data.clear();

    // Read 2-byte header
    uint8_t header[2];
    if (!readExact(clientFd, header, 2)) return false;

    uint8_t opcode = header[0] & 0x0F;
    bool    masked = (header[1] & 0x80) != 0;
    uint64_t plen  = header[1] & 0x7F;

    if (opcode == 0x8) return false;  // close frame

    // Extended payload length
    if (plen == 126) {
        uint8_t ext[2];
        if (!readExact(clientFd, ext, 2)) return false;
        plen = ((uint64_t)ext[0] << 8) | ext[1];
    } else if (plen == 127) {
        uint8_t ext[8];
        if (!readExact(clientFd, ext, 8)) return false;
        plen = 0;
        for (int i = 0; i < 8; ++i) plen = (plen << 8) | ext[i];
    }

    // Masking key (client→server frames are always masked)
    uint8_t maskKey[4] = {};
    if (masked) {
        if (!readExact(clientFd, maskKey, 4)) return false;
    }

    // Payload
    if (plen > 1024*1024) return false;  // sanity limit: 1MB
    data.resize((size_t)plen);
    if (plen > 0 && !readExact(clientFd, (uint8_t*)&data[0], (int)plen))
        return false;

    // Unmask
    if (masked)
        for (size_t i = 0; i < (size_t)plen; ++i)
            data[i] ^= maskKey[i % 4];

    return true;
}

// ══════════════════════════════════════════
//  WebSocket / HTTP handshake
// ══════════════════════════════════════════
bool WebSocketServer::doHandshake(const std::string& htmlContent) {
    std::string req = readHttpRequest();
    if (req.empty()) return false;

    if (isWebSocketUpgrade(req)) {
        std::string key = extractWebSocketKey(req);
        if (key.empty()) return false;

        // RFC 6455 Section 4.2.2 — the GUID is fixed by the standard
        // CORRECT: 258EAFA5-E914-47DA-95CA-C5AB0DC85B11
        std::string magic = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        uint8_t digest[20];
        sha1((const uint8_t*)magic.data(), magic.size(), digest);
        std::string accept = base64Encode(digest, 20);

        std::string response =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";

        return sendAll(clientFd, response.data(), (int)response.size());

    } else {
        // Serve the HTML page
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " + std::to_string(htmlContent.size()) + "\r\n"
            "Connection: close\r\n\r\n" + htmlContent;

        sendAll(clientFd, response.data(), (int)response.size());
        sockClose(clientFd);
        clientFd = -1;
        return false;
    }
}

// ══════════════════════════════════════════
//  Main server loop
// ══════════════════════════════════════════
bool WebSocketServer::listen(int port, const std::string& htmlFile,
                             MessageHandler onMessage) {
    // Load HTML into memory
    FILE* f = fopen(htmlFile.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "[server] Cannot open %s\n", htmlFile.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string html(flen, '\0');
    size_t nread = fread(&html[0], 1, flen, f);
    fclose(f);
    if ((long)nread != flen) {
        fprintf(stderr, "[server] Failed to read %s\n", htmlFile.c_str());
        return false;
    }

    // Create server socket
#ifdef _WIN32
    serverFd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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

    // Accept loop — handles HTTP and WebSocket connections
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        clientFd = (int)accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) continue;

        bool isWS = doHandshake(html);
        if (!isWS) continue;

        // WebSocket message loop for this client
        std::string msg;
        while (recvFrame(msg)) {
            if (!msg.empty() && onMessage)
                onMessage(msg);
        }

        sockClose(clientFd);
        clientFd = -1;
    }
    return true;
}

void WebSocketServer::send(const std::string& json) {
    if (clientFd >= 0) sendFrame(json);
}
