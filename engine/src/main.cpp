#include "sage/engine.h"

#include <string_view>

int main(int argc, char* argv[]) {
    sage::EngineConfig config;
    config.appName = "SAGE Engine v1.5.0 — GPU Prop Instancing, 3-CSM, Euphoria Ragdoll & Spline AI";
    config.windowWidth = 1280;
    config.windowHeight = 720;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--smoke-test") {
            config.maxFrames = 12;
            config.silentAudio = true;
        } else if (arg == "--scene" && i + 1 < argc) {
            config.scenePath = argv[++i];
        } else if (arg == "--mute") {
            config.silentAudio = true;
        }
    }

    sage::Engine engine(config);
    return engine.run();
}
