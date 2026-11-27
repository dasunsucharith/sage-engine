#include "sage/engine.h"

int main() {
    sage::EngineConfig config;
    config.appName = "SAGE Engine — Starter Sandbox";
    config.windowWidth = 1280;
    config.windowHeight = 720;

    sage::Engine engine(config);
    return engine.run();
}
