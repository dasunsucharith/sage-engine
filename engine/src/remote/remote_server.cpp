#include "sage/remote/remote_server.h"

#include <iostream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
static void closeSocket(SocketHandle s) { closesocket(s); }
static bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
static bool setNonBlocking(SocketHandle s) {
    u_long mode = 1;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
static void closeSocket(SocketHandle s) { close(s); }
static bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }
static bool setNonBlocking(SocketHandle s) {
    const int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
#endif

namespace sage {

namespace {
constexpr std::size_t kMaxLineBytes = 4u * 1024u * 1024u; // Requests are small; guard against junk
constexpr int kMaxClients = 8;
} // namespace

struct RemoteServer::Impl {
    struct Client {
        int id = 0;
        SocketHandle socket = kInvalidSocket;
        std::string inbox;
    };

    SocketHandle listener = kInvalidSocket;
    std::uint16_t port = 0;
    std::vector<Client> clients;
    int nextClientId = 1;
#ifdef _WIN32
    bool wsaStarted = false;
#endif

    void dropClient(std::size_t i) {
        closeSocket(clients[i].socket);
        std::cout << "[SAGE][Remote] Client " << clients[i].id << " disconnected\n";
        clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
    }
};

RemoteServer::RemoteServer() : m_impl(std::make_unique<Impl>()) {}

RemoteServer::~RemoteServer() { stop(); }

bool RemoteServer::start(std::uint16_t port) {
    stop();
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[SAGE][Remote] WSAStartup failed: remote control unavailable\n";
        return false;
    }
    m_impl->wsaStarted = true;
#endif
    SocketHandle s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kInvalidSocket) {
        std::cerr << "[SAGE][Remote] Could not create socket: remote control unavailable\n";
        stop();
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only: never reachable from the network
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(s, 4) != 0 || !setNonBlocking(s)) {
        std::cerr << "[SAGE][Remote] Could not listen on 127.0.0.1:" << port
                  << " (is another SAGE instance already using it? try --remote <otherPort>)\n";
        closeSocket(s);
        stop();
        return false;
    }
    m_impl->listener = s;
    m_impl->port = port;
    std::cout << "[SAGE][Remote] Remote control listening on 127.0.0.1:" << port
              << " (localhost only). MCP: python tools/sage_mcp/server.py\n";
    return true;
}

void RemoteServer::stop() {
    for (auto& c : m_impl->clients) closeSocket(c.socket);
    m_impl->clients.clear();
    if (m_impl->listener != kInvalidSocket) {
        closeSocket(m_impl->listener);
        m_impl->listener = kInvalidSocket;
    }
#ifdef _WIN32
    if (m_impl->wsaStarted) {
        WSACleanup();
        m_impl->wsaStarted = false;
    }
#endif
}

bool RemoteServer::isRunning() const { return m_impl->listener != kInvalidSocket; }

std::uint16_t RemoteServer::port() const { return m_impl->port; }

void RemoteServer::poll(std::vector<RemoteMessage>& out) {
    if (m_impl->listener == kInvalidSocket) return;

    // Accept any waiting clients.
    for (;;) {
        SocketHandle c = accept(m_impl->listener, nullptr, nullptr);
        if (c == kInvalidSocket) break;
        if (static_cast<int>(m_impl->clients.size()) >= kMaxClients || !setNonBlocking(c)) {
            closeSocket(c);
            continue;
        }
        m_impl->clients.push_back({m_impl->nextClientId++, c, {}});
        std::cout << "[SAGE][Remote] Client " << m_impl->clients.back().id << " connected\n";
    }

    // Read whatever has arrived and split it into complete lines.
    char buffer[16384];
    for (std::size_t i = 0; i < m_impl->clients.size();) {
        auto& client = m_impl->clients[i];
        bool dropped = false;
        for (;;) {
            const int n = static_cast<int>(recv(client.socket, buffer, sizeof(buffer), 0));
            if (n > 0) {
                client.inbox.append(buffer, static_cast<std::size_t>(n));
                if (client.inbox.size() > kMaxLineBytes) {
                    std::cerr << "[SAGE][Remote] Client " << client.id << " sent an oversized request; disconnecting\n";
                    dropped = true;
                    break;
                }
                continue;
            }
            if (n == 0 || !wouldBlock()) dropped = true; // Closed or error
            break;
        }
        std::size_t nl;
        while ((nl = client.inbox.find('\n')) != std::string::npos) {
            std::string line = client.inbox.substr(0, nl);
            client.inbox.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) out.push_back({client.id, std::move(line)});
        }
        if (dropped) {
            m_impl->dropClient(i);
        } else {
            ++i;
        }
    }
}

void RemoteServer::send(int clientId, const std::string& line) {
    for (std::size_t i = 0; i < m_impl->clients.size(); ++i) {
        auto& client = m_impl->clients[i];
        if (client.id != clientId) continue;
        std::string data = line;
        data.push_back('\n');
        std::size_t sent = 0;
        int spins = 0;
        while (sent < data.size()) {
            const int n = static_cast<int>(::send(client.socket, data.data() + sent, static_cast<int>(data.size() - sent), 0));
            if (n > 0) {
                sent += static_cast<std::size_t>(n);
            } else if (n < 0 && wouldBlock() && ++spins < 200000) {
                continue; // Socket buffer full (large screenshot path list / logs): retry briefly
            } else {
                m_impl->dropClient(i);
                return;
            }
        }
        return;
    }
}

} // namespace sage
