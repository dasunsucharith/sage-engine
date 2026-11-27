#include "sage/engine.h"

#include <string_view>

int main(int argc, char* argv[]) {
    sage::EngineConfig config;
    config.appName = "SAGE Engine v0.3 — 3D Stunt & Delivery Sandbox";
    config.windowWidth = 1280;
    config.windowHeight = 720;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--smoke-test") {
            config.maxFrames = 10;
        } else if (arg == "--scene" && i + 1 < argc) {
            config.scenePath = argv[++i];
        }
    }

    sage::Engine engine(config);
    return engine.run();
}
