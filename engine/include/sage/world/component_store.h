#pragma once

// ComponentStore<T>: optional per-entity data, stored only for entities that have it.
//
// A "sparse set": `sparse[slot]` gives the index of the entity's component in the tightly
// packed `dense` arrays (or kNone). Lookup, insert and remove are O(1), and iterating a
// component type touches only the entities that actually have it, in contiguous memory.
// Entities are identified by their stable slot number (see EntityHandle in world.h).

#include <cstdint>
#include <utility>
#include <vector>

namespace sage {

template <typename T>
class ComponentStore {
public:
    static constexpr std::uint32_t kNone = 0xFFFFFFFFu;

    // Returns the component for `slot`, creating a default-constructed one if missing.
    T& getOrAdd(std::uint32_t slot) {
        if (slot >= m_sparse.size()) {
            m_sparse.resize(static_cast<std::size_t>(slot) + 1, kNone);
        }
        if (m_sparse[slot] == kNone) {
            m_sparse[slot] = static_cast<std::uint32_t>(m_dense.size());
            m_dense.emplace_back();
            m_denseSlots.push_back(slot);
        }
        return m_dense[m_sparse[slot]];
    }

    T* get(std::uint32_t slot) {
        return (slot < m_sparse.size() && m_sparse[slot] != kNone) ? &m_dense[m_sparse[slot]] : nullptr;
    }
    const T* get(std::uint32_t slot) const {
        return (slot < m_sparse.size() && m_sparse[slot] != kNone) ? &m_dense[m_sparse[slot]] : nullptr;
    }

    // Removes the component for `slot` (no-op if missing). Keeps `dense` packed by moving
    // the last component into the freed position.
    void remove(std::uint32_t slot) {
        if (slot >= m_sparse.size() || m_sparse[slot] == kNone) return;
        const std::uint32_t idx = m_sparse[slot];
        const std::uint32_t last = static_cast<std::uint32_t>(m_dense.size() - 1);
        if (idx != last) {
            m_dense[idx] = std::move(m_dense[last]);
            m_denseSlots[idx] = m_denseSlots[last];
            m_sparse[m_denseSlots[idx]] = idx;
        }
        m_dense.pop_back();
        m_denseSlots.pop_back();
        m_sparse[slot] = kNone;
    }

    void clear() {
        m_sparse.clear();
        m_dense.clear();
        m_denseSlots.clear();
    }

    std::size_t size() const { return m_dense.size(); }

    // Iterate as: for (std::size_t i = 0; i < size(); ++i) { slotAt(i); at(i); }
    std::uint32_t slotAt(std::size_t i) const { return m_denseSlots[i]; }
    T& at(std::size_t i) { return m_dense[i]; }
    const T& at(std::size_t i) const { return m_dense[i]; }

private:
    std::vector<std::uint32_t> m_sparse;     // slot -> dense index (kNone if absent)
    std::vector<T> m_dense;                  // packed components
    std::vector<std::uint32_t> m_denseSlots; // dense index -> slot
};

} // namespace sage
