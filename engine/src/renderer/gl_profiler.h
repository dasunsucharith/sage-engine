#pragma once

// Internal renderer module (OpenGL backend). Not part of the public engine API.

#include "renderer/gl_functions.h"
#include "sage/renderer/renderer.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace sage::render {

// ---------------------------------------------------------------------------
// GPU pass profiler (GL_TIME_ELAPSED timer queries)
// ---------------------------------------------------------------------------
// Reading a GPU timer right after issuing it forces the CPU to wait for the GPU,
// which would distort the very thing we measure. Instead we keep a ring of 4 frames
// of queries and read a frame's results only when it comes around again (~3 frames
// later) and only if the GPU reports them available; otherwise that sample is dropped.
// GL_TIME_ELAPSED queries cannot nest, so passes are strictly sequential: begin()
// automatically ends the previous pass.
class GpuPassProfiler {
public:
    static constexpr int kRing = 4;
    static constexpr int kMaxPasses = 16;

    void initialize() {
        m_enabled = gl::hasTimerQueries();
        if (!m_enabled) return;
        gl::genQueries(kRing * kMaxPasses, m_queries.data());
    }

    void shutdown() {
        if (m_enabled) {
            gl::deleteQueries(kRing * kMaxPasses, m_queries.data());
        }
        m_enabled = false;
    }

    void beginFrame() {
        // CPU side (always on): how long the renderer spends issuing each pass.
        m_cpuCurrent.clear();
        m_cpuPassName = nullptr;
        if (!m_enabled || m_inFrame) return;
        m_inFrame = true;
        Slot& slot = m_slots[static_cast<std::size_t>(m_slotIndex)];
        if (slot.pending) {
            collect(m_slotIndex);
        }
        slot.count = 0;
        slot.pending = false;
    }

    // Starts timing a named pass (ends the previous one). `name` must be a string literal.
    void begin(const char* name) {
        closeCpuPass();
        m_cpuPassName = name;
        m_cpuPassStart = std::chrono::steady_clock::now();
        if (!m_enabled || !m_inFrame) return;
        Slot& slot = m_slots[static_cast<std::size_t>(m_slotIndex)];
        if (m_passOpen) {
            gl::endQuery(GL_TIME_ELAPSED);
            m_passOpen = false;
        }
        if (slot.count >= kMaxPasses) return;
        gl::beginQuery(GL_TIME_ELAPSED, queryFor(m_slotIndex, slot.count));
        slot.names[static_cast<std::size_t>(slot.count)] = name;
        ++slot.count;
        m_passOpen = true;
    }

    void endFrame() {
        closeCpuPass();
        m_cpuLast = m_cpuCurrent;
        if (!m_enabled || !m_inFrame) return;
        if (m_passOpen) {
            gl::endQuery(GL_TIME_ELAPSED);
            m_passOpen = false;
        }
        Slot& slot = m_slots[static_cast<std::size_t>(m_slotIndex)];
        slot.pending = slot.count > 0;
        m_slotIndex = (m_slotIndex + 1) % kRing;
        m_inFrame = false;
    }

    const std::vector<GpuPassTiming>& last() const { return m_last; }
    const std::vector<TimingSample>& lastCpu() const { return m_cpuLast; }
    std::uint64_t completedFrames() const { return m_completedFrames; }

private:
    struct Slot {
        std::array<const char*, kMaxPasses> names{};
        int count = 0;
        bool pending = false;
    };

    void closeCpuPass() {
        if (!m_cpuPassName) return;
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_cpuPassStart).count();
        m_cpuCurrent.push_back({m_cpuPassName, static_cast<float>(ms)});
        m_cpuPassName = nullptr;
    }

    GLuint queryFor(int slot, int pass) const {
        return m_queries[static_cast<std::size_t>(slot * kMaxPasses + pass)];
    }

    void collect(int slotIdx) {
        const Slot& slot = m_slots[static_cast<std::size_t>(slotIdx)];
        // The last query of a frame finishes last on the GPU: if it is ready, all are.
        GLint available = 0;
        gl::getQueryObjectiv(queryFor(slotIdx, slot.count - 1), GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available) {
            ++m_droppedFrames; // Drop this sample rather than stall the CPU.
            return;
        }
        m_last.clear();
        for (int i = 0; i < slot.count; ++i) {
            GLuint64Sage ns = 0;
            gl::getQueryObjectui64v(queryFor(slotIdx, i), GL_QUERY_RESULT, &ns);
            m_last.push_back({slot.names[static_cast<std::size_t>(i)], static_cast<float>(static_cast<double>(ns) / 1.0e6)});
        }
        ++m_completedFrames;
    }

    bool m_enabled = false;
    bool m_inFrame = false;
    bool m_passOpen = false;
    int m_slotIndex = 0;
    std::array<GLuint, kRing * kMaxPasses> m_queries{};
    std::array<Slot, kRing> m_slots{};
    std::vector<GpuPassTiming> m_last;
    std::uint64_t m_completedFrames = 0;
    std::uint64_t m_droppedFrames = 0;
    const char* m_cpuPassName = nullptr;
    std::chrono::steady_clock::time_point m_cpuPassStart{};
    std::vector<TimingSample> m_cpuCurrent;
    std::vector<TimingSample> m_cpuLast;
};

// Ends the profiler frame however renderWorld() exits (including early returns).
struct GpuProfilerFrameScope {
    explicit GpuProfilerFrameScope(GpuPassProfiler& p) : profiler(p) { profiler.beginFrame(); }
    ~GpuProfilerFrameScope() { profiler.endFrame(); }
    GpuProfilerFrameScope(const GpuProfilerFrameScope&) = delete;
    GpuProfilerFrameScope& operator=(const GpuProfilerFrameScope&) = delete;
    GpuPassProfiler& profiler;
};

} // namespace sage::render
