#include "sage/platform/window.h"

#include "sage/platform/input.h"

#include <SDL3/SDL.h>

#include <iostream>

namespace sage {

namespace {

Key translateSDLScancode(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_A: return Key::A;
        case SDL_SCANCODE_B: return Key::B;
        case SDL_SCANCODE_C: return Key::C;
        case SDL_SCANCODE_D: return Key::D;
        case SDL_SCANCODE_E: return Key::E;
        case SDL_SCANCODE_F: return Key::F;
        case SDL_SCANCODE_G: return Key::G;
        case SDL_SCANCODE_H: return Key::H;
        case SDL_SCANCODE_I: return Key::I;
        case SDL_SCANCODE_J: return Key::J;
        case SDL_SCANCODE_K: return Key::K;
        case SDL_SCANCODE_L: return Key::L;
        case SDL_SCANCODE_M: return Key::M;
        case SDL_SCANCODE_N: return Key::N;
        case SDL_SCANCODE_O: return Key::O;
        case SDL_SCANCODE_P: return Key::P;
        case SDL_SCANCODE_Q: return Key::Q;
        case SDL_SCANCODE_R: return Key::R;
        case SDL_SCANCODE_S: return Key::S;
        case SDL_SCANCODE_T: return Key::T;
        case SDL_SCANCODE_U: return Key::U;
        case SDL_SCANCODE_V: return Key::V;
        case SDL_SCANCODE_W: return Key::W;
        case SDL_SCANCODE_X: return Key::X;
        case SDL_SCANCODE_Y: return Key::Y;
        case SDL_SCANCODE_Z: return Key::Z;
        case SDL_SCANCODE_UP: return Key::Up;
        case SDL_SCANCODE_DOWN: return Key::Down;
        case SDL_SCANCODE_LEFT: return Key::Left;
        case SDL_SCANCODE_RIGHT: return Key::Right;
        case SDL_SCANCODE_SPACE: return Key::Space;
        case SDL_SCANCODE_ESCAPE: return Key::Escape;
        case SDL_SCANCODE_RETURN: return Key::Enter;
        case SDL_SCANCODE_TAB: return Key::Tab;
        case SDL_SCANCODE_LSHIFT: return Key::LeftShift;
        case SDL_SCANCODE_RSHIFT: return Key::RightShift;
        case SDL_SCANCODE_LCTRL: return Key::LeftControl;
        case SDL_SCANCODE_RCTRL: return Key::RightControl;
        case SDL_SCANCODE_F1: return Key::F1;
        case SDL_SCANCODE_F2: return Key::F2;
        case SDL_SCANCODE_F3: return Key::F3;
        case SDL_SCANCODE_F4: return Key::F4;
        case SDL_SCANCODE_F5: return Key::F5;
        case SDL_SCANCODE_F6: return Key::F6;
        case SDL_SCANCODE_F7: return Key::F7;
        case SDL_SCANCODE_F8: return Key::F8;
        case SDL_SCANCODE_F9: return Key::F9;
        case SDL_SCANCODE_F10: return Key::F10;
        case SDL_SCANCODE_F11: return Key::F11;
        case SDL_SCANCODE_F12: return Key::F12;
        default: return Key::Unknown;
    }
}

} // namespace

Window::~Window() {
    shutdown();
}

bool Window::initialize(const WindowConfig& config) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "[SAGE][Platform] SDL_Init(SDL_INIT_VIDEO) failed: " << SDL_GetError() << "\n";
        return false;
    }
    m_sdlVideoInitialized = true;

    // Request a modern OpenGL 3.3 Core Profile context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    m_window = SDL_CreateWindow(config.title.c_str(), config.width, config.height, flags);
    if (!m_window) {
        std::cerr << "[SAGE][Platform] SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        shutdown();
        return false;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) {
        std::cerr << "[SAGE][Platform] SDL_GL_CreateContext (OpenGL 3.3 Core) failed: "
                  << SDL_GetError() << "\n";
        shutdown();
        return false;
    }

    if (!SDL_GL_MakeCurrent(m_window, m_glContext)) {
        std::cerr << "[SAGE][Platform] SDL_GL_MakeCurrent failed: " << SDL_GetError() << "\n";
        shutdown();
        return false;
    }

    if (!SDL_GL_SetSwapInterval(config.vsync ? 1 : 0)) {
        std::cerr << "[SAGE][Platform] Warning: failed to set VSync ("
                  << (config.vsync ? "ON" : "OFF") << "): " << SDL_GetError() << "\n";
    }

    SDL_GetWindowSizeInPixels(m_window, &m_width, &m_height);
    m_shouldClose = false;
    m_wasResized = false;
    return true;
}

void Window::shutdown() {
    if (m_glContext) {
        SDL_GL_DestroyContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_sdlVideoInitialized) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        SDL_Quit();
        m_sdlVideoInitialized = false;
    }
}

void Window::pollEvents(InputState& input) {
    input.beginFrame();

    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                m_shouldClose = true;
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_RESIZED:
                if (m_window) {
                    SDL_GetWindowSizeInPixels(m_window, &m_width, &m_height);
                    m_wasResized = true;
                }
                break;

            case SDL_EVENT_KEY_DOWN:
                if (!event.key.repeat) {
                    input.setKeyState(translateSDLScancode(event.key.scancode), true);
                }
                break;

            case SDL_EVENT_KEY_UP:
                input.setKeyState(translateSDLScancode(event.key.scancode), false);
                break;

            case SDL_EVENT_MOUSE_MOTION:
                input.setMousePosition(event.motion.x, event.motion.y);
                input.addMouseDelta(event.motion.xrel, event.motion.yrel);
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const bool down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                if (event.button.button == SDL_BUTTON_LEFT) {
                    input.setMouseButtonState(MouseButton::Left, down);
                } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    input.setMouseButtonState(MouseButton::Middle, down);
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    input.setMouseButtonState(MouseButton::Right, down);
                }
                break;
            }

            case SDL_EVENT_MOUSE_WHEEL:
                input.addMouseWheel(event.wheel.y);
                break;

            default:
                break;
        }
    }
}

void Window::swapBuffers() {
    if (m_window) {
        SDL_GL_SwapWindow(m_window);
    }
}

void Window::setTitle(const std::string& title) {
    if (m_window) {
        SDL_SetWindowTitle(m_window, title.c_str());
    }
}

double Window::getTimeSeconds() {
    return static_cast<double>(SDL_GetPerformanceCounter()) /
           static_cast<double>(SDL_GetPerformanceFrequency());
}

void* Window::getGLProcAddress(const char* procName) {
    return reinterpret_cast<void*>(SDL_GL_GetProcAddress(procName));
}

} // namespace sage
