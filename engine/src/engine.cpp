#include "sage/engine.h"

#include <iostream>

namespace sage {

Engine::Engine(const EngineConfig& config)
    : m_config(config) {}

Engine::~Engine() = default;

int Engine::run() {
    m_running = true;

    std::cout << "========================================\n";
    std::cout << "  SAGE Engine v0.0.1 (Bootstrap)\n";
    std::cout << "  App: " << m_config.appName << "\n";
    std::cout << "  Target Window: " << m_config.windowWidth << "x" << m_config.windowHeight << "\n";
    std::cout << "========================================\n";
    std::cout << "[SAGE] Core initialized cleanly. Next milestone (v0.1): SDL3 Window & GPU Renderer!\n";

    m_running = false;
    return 0;
}

} // namespace sage
