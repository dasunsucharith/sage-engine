#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace sage {

// Platform-independent key codes (maps directly to Lua input.isKeyDown("W"))
enum class Key : std::size_t {
    Unknown = 0,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Up, Down, Left, Right,
    Space, Escape, Enter, Tab,
    LeftShift, RightShift, LeftControl, RightControl,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Count
};

enum class MouseButton : std::size_t {
    Left = 0,
    Middle,
    Right,
    Count
};

// Case-insensitive lookup for scripting and config files (e.g. "W", "Space", "Escape")
Key keyFromName(std::string_view name);

// Stores keyboard and mouse input state for the current frame.
class InputState {
public:
    void beginFrame();

    void setKeyState(Key key, bool isDown);
    void setMouseButtonState(MouseButton button, bool isDown);
    void addMouseDelta(float dx, float dy);
    void setMousePosition(float x, float y);
    void addMouseWheel(float dy);

    bool isKeyDown(Key key) const;
    bool wasKeyPressed(Key key) const;
    bool wasKeyReleased(Key key) const;

    bool isMouseButtonDown(MouseButton button) const;
    bool wasMouseButtonPressed(MouseButton button) const;

    float mouseX() const { return m_mouseX; }
    float mouseY() const { return m_mouseY; }
    float mouseDeltaX() const { return m_mouseDeltaX; }
    float mouseDeltaY() const { return m_mouseDeltaY; }
    float mouseWheelDelta() const { return m_mouseWheelDelta; }

private:
    static constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);
    static constexpr std::size_t kMouseButtonCount = static_cast<std::size_t>(MouseButton::Count);

    std::array<bool, kKeyCount> m_currentKeys{};
    std::array<bool, kKeyCount> m_previousKeys{};
    std::array<bool, kMouseButtonCount> m_currentMouseButtons{};
    std::array<bool, kMouseButtonCount> m_previousMouseButtons{};
    float m_mouseX = 0.0f;
    float m_mouseY = 0.0f;
    float m_mouseDeltaX = 0.0f;
    float m_mouseDeltaY = 0.0f;
    float m_mouseWheelDelta = 0.0f;
};

} // namespace sage
