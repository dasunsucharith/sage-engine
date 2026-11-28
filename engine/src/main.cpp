#include "sage/engine.h"
#include "sage/version.h"

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>

namespace {

// Parses an optional positive integer argument following a flag (e.g. `--benchmark 900`).
// Returns true and advances `i` only when the next argv entry is a number.
bool parseOptionalCount(int argc, char* argv[], int& i, int& outValue) {
    if (i + 1 >= argc) {
        return false;
    }
    const std::string_view next(argv[i + 1]);
    int value = 0;
    const auto [ptr, ec] = std::from_chars(next.data(), next.data() + next.size(), value);
    if (ec != std::errc{} || ptr != next.data() + next.size() || value <= 0) {
        return false;
    }
    outValue = value;
    ++i;
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    sage::EngineConfig config;
    config.appName = std::string("SAGE Engine v") + sage::kEngineVersion;
    config.windowWidth = 1280;
    config.windowHeight = 720;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--smoke-test") {
            // Quick "does it boot & render" check. NOT a performance measurement:
            // 12 frames are dominated by the cold-start hitch. Use --benchmark for numbers.
            config.maxFrames = 12;
            config.warmupFrames = 2;
            config.silentAudio = true;
        } else if (arg == "--benchmark") {
            // Steady-state performance run: warm up, then measure N frames with VSync OFF
            // so frame times reflect real CPU+GPU cost instead of the monitor refresh rate.
            int measured = 600;
            parseOptionalCount(argc, argv, i, measured);
            config.benchmarkMode = true;
            config.warmupFrames = 120;
            // +1: frame 0 has no previous frame to measure against.
            config.maxFrames = config.warmupFrames + measured + 1;
            config.vsync = false;
            config.silentAudio = true;
        } else if (arg == "--scene" && i + 1 < argc) {
            config.scenePath = argv[++i];
        } else if (arg == "--resource" && i + 1 < argc) {
            config.onlyResource = argv[++i];
        } else if (arg == "--survival") {
            config.scenePath = "scenes/survival_island.json";
            config.onlyResource = "survival_game";
            config.appName = std::string("Wild Frontier: Echo Island (SAGE Engine v") + sage::kEngineVersion + ")";
        } else if (arg == "--mute") {
            config.silentAudio = true;
        } else if (arg == "--no-vsync") {
            config.vsync = false;
        } else if (arg == "--screenshot" && i + 1 < argc) {
            config.screenshotPath = argv[++i];
        } else if (arg == "--cam" && i + 6 < argc) {
            try {
                config.overrideCamPos = {std::stof(argv[i + 1]), std::stof(argv[i + 2]), std::stof(argv[i + 3])};
                config.overrideCamTarget = {std::stof(argv[i + 4]), std::stof(argv[i + 5]), std::stof(argv[i + 6])};
                config.overrideCamera = true;
            } catch (const std::exception&) {
                std::cerr << "[SAGE] --cam expects 6 numbers: --cam px py pz tx ty tz (ignored)\n";
            }
            i += 6;
        } else {
            std::cerr << "[SAGE] Unknown argument ignored: " << arg << "\n";
        }
    }

    sage::Engine engine(config);
    return engine.run();
}
