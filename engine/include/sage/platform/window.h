#pragma once

#include <string>

struct SDL_Window;
struct SDL_GLContextState;

namespace sage {

class InputState;

struct WindowConfig {
    std::string title = "SAGE Engine";
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool resizable = true;
};

// Cross-platform OS window and OpenGL context wrapper powered by SDL3.
// Keeps SDL types encapsulated so the rest of SAGE stays platform-agnostic.
class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool initialize(const WindowConfig& config);
    void shutdown();

    void pollEvents(InputState& input);
    void swapBuffers();
    void setTitle(const std::string& title);

    bool shouldClose() const { return m_shouldClose; }
    void requestClose() { m_shouldClose = true; }

    int width() const { return m_width; }
    int height() const { return m_height; }
    bool wasResized() const { return m_wasResized; }
    void clearResizedFlag() { m_wasResized = false; }

    // High-resolution monotonic clock in seconds (powered by SDL3 performance counter)
    static double getTimeSeconds();

    // Loads an OpenGL function pointer from the active graphics driver
    static void* getGLProcAddress(const char* procName);

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContextState* m_glContext = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_shouldClose = false;
    bool m_wasResized = false;
    bool m_sdlVideoInitialized = false;
};

} // namespace sage
