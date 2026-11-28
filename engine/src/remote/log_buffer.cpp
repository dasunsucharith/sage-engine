#include "sage/remote/log_buffer.h"

#include <deque>
#include <iostream>
#include <mutex>
#include <streambuf>

namespace sage {

namespace {

// Forwards every character to the original stream buffer and hands complete lines to a sink.
class TeeBuf : public std::streambuf {
public:
    using Sink = void (*)(void* ctx, bool isError, std::string&& line);

    TeeBuf(std::streambuf* original, bool isError, Sink sink, void* ctx)
        : m_original(original), m_isError(isError), m_sink(sink), m_ctx(ctx) {}

    std::streambuf* original() const { return m_original; }

protected:
    int overflow(int ch) override {
        std::lock_guard<std::recursive_mutex> lock(m_lock); // Other threads may log too
        if (ch == traits_type::eof()) return traits_type::not_eof(ch);
        const char c = static_cast<char>(ch);
        if (c == '\n') {
            m_sink(m_ctx, m_isError, std::move(m_line));
            m_line.clear();
        } else if (c != '\r' && m_line.size() < 4096) {
            m_line.push_back(c);
        }
        return m_original ? m_original->sputc(c) : ch;
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        for (std::streamsize i = 0; i < n; ++i) overflow(static_cast<unsigned char>(s[i]));
        return n;
    }
    int sync() override { return m_original ? m_original->pubsync() : 0; }

private:
    std::streambuf* m_original;
    bool m_isError;
    Sink m_sink;
    void* m_ctx;
    std::string m_line;
    std::recursive_mutex m_lock;
};

} // namespace

struct LogBuffer::Impl {
    std::size_t capacity;
    mutable std::mutex mutex; // Audio/physics threads may log too
    std::deque<LogLine> lines;
    std::uint64_t seq = 0;
    std::unique_ptr<TeeBuf> outTee;
    std::unique_ptr<TeeBuf> errTee;

    static void sink(void* ctx, bool isError, std::string&& text) {
        auto* self = static_cast<Impl*>(ctx);
        std::lock_guard<std::mutex> lock(self->mutex);
        self->lines.push_back({++self->seq, isError, std::move(text)});
        while (self->lines.size() > self->capacity) self->lines.pop_front();
    }
};

LogBuffer::LogBuffer(std::size_t capacity) : m_impl(std::make_unique<Impl>()) {
    m_impl->capacity = capacity;
}

LogBuffer::~LogBuffer() {
    if (m_impl->outTee) std::cout.rdbuf(m_impl->outTee->original());
    if (m_impl->errTee) std::cerr.rdbuf(m_impl->errTee->original());
}

void LogBuffer::attach() {
    if (m_impl->outTee) return;
    m_impl->outTee = std::make_unique<TeeBuf>(std::cout.rdbuf(), false, &Impl::sink, m_impl.get());
    m_impl->errTee = std::make_unique<TeeBuf>(std::cerr.rdbuf(), true, &Impl::sink, m_impl.get());
    std::cout.rdbuf(m_impl->outTee.get());
    std::cerr.rdbuf(m_impl->errTee.get());
}

std::vector<LogLine> LogBuffer::linesAfter(std::uint64_t afterSeq, std::size_t limit) const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    std::vector<LogLine> out;
    for (const LogLine& l : m_impl->lines) {
        if (l.seq > afterSeq) out.push_back(l);
    }
    if (out.size() > limit) out.erase(out.begin(), out.end() - static_cast<std::ptrdiff_t>(limit));
    return out;
}

std::uint64_t LogBuffer::lastSeq() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->seq;
}

} // namespace sage
