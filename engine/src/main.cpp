#include "sage/engine.h"

#include <string_view>

int main(int argc, char* argv[]) {
    sage::EngineConfig config;
    config.appName = "SAGE Engine v1.6.0 — Release PBR, Material-Merged Instancing, Soft Water & Entity-Space IK";
    config.windowWidth = 1280;
    config.windowHeight = 720;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--smoke-test") {
            config.maxFrames = 12;
            config.silentAudio = true;
        } else if (arg == "--scene" && i + 1 < argc) {
            config.scenePath = argv[++i];
        } else if (arg == "--resource" && i + 1 < argc) {
            config.onlyResource = argv[++i];
        } else if (arg == "--survival") {
            config.scenePath = "scenes/survival_island.json";
            config.onlyResource = "survival_game";
            config.appName = "Wild Frontier: Echo Island — Open-World Survival (SAGE Engine v1.6.0)";
        } else if (arg == "--mute") {
            config.silentAudio = true;
        } else if (arg == "--screenshot" && i + 1 < argc) {
            config.screenshotPath = argv[++i];
        } else if (arg == "--cam" && i + 6 < argc) {
            config.overrideCamera = true;
            config.overrideCamPos = {std::stof(argv[i + 1]), std::stof(argv[i + 2]), std::stof(argv[i + 3])};
            config.overrideCamTarget = {std::stof(argv[i + 4]), std::stof(argv[i + 5]), std::stof(argv[i + 6])};
            i += 6;
        }
    }

    sage::Engine engine(config);
    return engine.run();
}
