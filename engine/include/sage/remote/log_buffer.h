#pragma once

// LogBuffer: keeps the most recent engine log lines (everything written to std::cout and
// std::cerr) in memory, so remote tools (the MCP bridge) can read them. Output still goes
// to the console exactly as before: the buffer "tees" the streams.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sage {

struct LogLine {
    std::uint64_t seq = 0; // Increasing number, so readers can ask "everything after N"
    bool isError = false;  // Written to std::cerr
    std::string text;
};

class LogBuffer {
public:
    explicit LogBuffer(std::size_t capacity = 2000);
    ~LogBuffer(); // Restores std::cout / std::cerr

    // Starts capturing std::cout and std::cerr (idempotent).
    void attach();

    // Lines with seq > afterSeq, oldest first, at most `limit` (the newest ones if more).
    std::vector<LogLine> linesAfter(std::uint64_t afterSeq, std::size_t limit) const;
    std::uint64_t lastSeq() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace sage
