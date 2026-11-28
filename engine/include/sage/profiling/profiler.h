#pragma once

// Lightweight CPU profiling for the engine loop.
//
// Usage:
//     profiler.beginFrame();
//     { CpuScope s(profiler, "physics"); physics.step(...); }
//     profiler.endFrame();
//     for (const TimingSample& t : profiler.last()) { ... }
//
// Scopes may nest. Each section reports EXCLUSIVE time (its own time minus its
// children), so sections add up to the frame and nothing is counted twice. Time
// not covered by any scope is reported as "other". Section names must be string
// literals (stored as pointers; compared by content).

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>
#include <vector>

namespace sage {

// One named duration in milliseconds (CPU section or GPU render pass).
struct TimingSample {
    const char* name = "";
    float ms = 0.0f;
};

class CpuProfiler {
public:
    using Clock = std::chrono::steady_clock;

    void beginFrame() {
        m_stack.clear();
        m_accum.clear();
        m_frameStart = Clock::now();
        m_inFrame = true;
    }

    void push(const char* name) {
        if (!m_inFrame) return;
        m_stack.push_back({name, Clock::now(), 0.0});
    }

    void pop() {
        if (!m_inFrame || m_stack.empty()) return;
        const Open open = m_stack.back();
        m_stack.pop_back();
        const double totalMs = msSince(open.start);
        add(open.name, totalMs - open.childMs);
        if (!m_stack.empty()) {
            m_stack.back().childMs += totalMs;
        }
    }

    void endFrame() {
        if (!m_inFrame) return;
        while (!m_stack.empty()) {
            pop(); // Close anything left open so the frame always balances.
        }
        m_lastFrameMs = static_cast<float>(msSince(m_frameStart));
        double covered = 0.0;
        m_last.clear();
        for (const auto& [name, ms] : m_accum) {
            m_last.push_back({name, static_cast<float>(ms)});
            covered += ms;
        }
        m_last.push_back({"other", static_cast<float>(std::max(0.0, static_cast<double>(m_lastFrameMs) - covered))});
        m_inFrame = false;
        ++m_frameId;
    }

    const std::vector<TimingSample>& last() const { return m_last; }
    float lastFrameMs() const { return m_lastFrameMs; }
    unsigned long long frameId() const { return m_frameId; }

private:
    struct Open {
        const char* name;
        Clock::time_point start;
        double childMs;
    };

    static double msSince(Clock::time_point t) {
        return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
    }

    void add(const char* name, double ms) {
        for (auto& [n, total] : m_accum) {
            if (n == name || std::strcmp(n, name) == 0) {
                total += ms;
                return;
            }
        }
        m_accum.emplace_back(name, ms);
    }

    bool m_inFrame = false;
    Clock::time_point m_frameStart{};
    std::vector<Open> m_stack;
    std::vector<std::pair<const char*, double>> m_accum; // First-seen order
    std::vector<TimingSample> m_last;
    float m_lastFrameMs = 0.0f;
    unsigned long long m_frameId = 0;
};

// RAII section: times everything until the end of the enclosing block.
class CpuScope {
public:
    CpuScope(CpuProfiler& profiler, const char* name) : m_profiler(profiler) { m_profiler.push(name); }
    ~CpuScope() { m_profiler.pop(); }
    CpuScope(const CpuScope&) = delete;
    CpuScope& operator=(const CpuScope&) = delete;

private:
    CpuProfiler& m_profiler;
};

} // namespace sage
