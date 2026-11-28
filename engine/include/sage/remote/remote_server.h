#pragma once

// RemoteServer: a tiny localhost-only TCP server for the SAGE remote-control protocol.
//
// Off unless the engine is started with --remote. It binds 127.0.0.1 only (never the
// network), uses non-blocking sockets, and is polled once per frame on the main thread,
// so commands never race with rendering, physics or Lua. Messages are one JSON object
// per line (newline-delimited). See docs/mcp.md for the command reference.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sage {

struct RemoteMessage {
    int clientId = 0;
    std::string line; // One complete JSON request (without the trailing newline)
};

class RemoteServer {
public:
    RemoteServer();
    ~RemoteServer();
    RemoteServer(const RemoteServer&) = delete;
    RemoteServer& operator=(const RemoteServer&) = delete;

    // Starts listening on 127.0.0.1:port. Logs a clear message and returns false on failure.
    bool start(std::uint16_t port);
    void stop();
    bool isRunning() const;
    std::uint16_t port() const;

    // Accepts new clients and collects complete request lines (non-blocking).
    void poll(std::vector<RemoteMessage>& out);
    // Sends one response line (a newline is appended) to a client.
    void send(int clientId, const std::string& line);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sage
