#pragma once

// Small helpers shared by the SAGE tests.

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace sage_test {

inline bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// A unique, empty temporary folder that deletes itself when the test ends.
class TempDir {
public:
    TempDir() {
        static std::atomic<int> counter{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("sage_test_" + std::to_string(stamp) + "_" + std::to_string(counter++));
        std::filesystem::create_directories(m_path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(m_path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return m_path; }

    // Writes `text` to a file inside the folder (creating sub-folders) and returns its path.
    std::filesystem::path write(const std::string& relative, const std::string& text) const {
        const std::filesystem::path p = m_path / relative;
        std::filesystem::create_directories(p.parent_path());
        std::ofstream(p, std::ios::binary) << text;
        return p;
    }

private:
    std::filesystem::path m_path;
};

} // namespace sage_test
