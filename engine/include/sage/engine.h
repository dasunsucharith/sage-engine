#pragma once

#include <string>

namespace sage {

struct EngineConfig {
    std::string appName = "SAGE Sandbox";
    int windowWidth = 1280;
    int windowHeight = 720;
    bool vsync = true;
};

class Engine {
public:
    explicit Engine(const EngineConfig& config);
    ~Engine();

    // Starts the engine and runs the main loop (Window + Graphics coming in v0.1!)
    int run();

private:
    EngineConfig m_config;
    bool m_running = false;
};

} // namespace sage
