#include "sage/platform/input.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace sage {

namespace {

std::string toLowerCopy(std::string_view input) {
    std::string out(input);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return out;
}

} // namespace

Key keyFromName(std::string_view name) {
    const std::string lower = toLowerCopy(name);
    if (lower.size() == 1 && lower[0] >= 'a' && lower[0] <= 'z') {
        const auto offset = static_cast<std::size_t>(lower[0] - 'a');
        return static_cast<Key>(static_cast<std::size_t>(Key::A) + offset);
    }

    if (lower == "up") return Key::Up;
    if (lower == "down") return Key::Down;
    if (lower == "left") return Key::Left;
    if (lower == "right") return Key::Right;
    if (lower == "space") return Key::Space;
    if (lower == "escape" || lower == "esc") return Key::Escape;
    if (lower == "enter" || lower == "return") return Key::Enter;
    if (lower == "tab") return Key::Tab;
    if (lower == "lshift" || lower == "leftshift" || lower == "shift") return Key::LeftShift;
    if (lower == "rshift" || lower == "rightshift") return Key::RightShift;
    if (lower == "lctrl" || lower == "leftcontrol" || lower == "ctrl") return Key::LeftControl;
    if (lower == "rctrl" || lower == "rightcontrol") return Key::RightControl;

    if (lower == "f1") return Key::F1;
    if (lower == "f2") return Key::F2;
    if (lower == "f3") return Key::F3;
    if (lower == "f4") return Key::F4;
    if (lower == "f5") return Key::F5;
    if (lower == "f6") return Key::F6;
    if (lower == "f7") return Key::F7;
    if (lower == "f8") return Key::F8;
    if (lower == "f9") return Key::F9;
    if (lower == "f10") return Key::F10;
    if (lower == "f11") return Key::F11;
    if (lower == "f12") return Key::F12;

    return Key::Unknown;
}

void InputState::beginFrame() {
    m_previousKeys = m_currentKeys;
    m_previousMouseButtons = m_currentMouseButtons;
    m_mouseDeltaX = 0.0f;
    m_mouseDeltaY = 0.0f;
    m_mouseWheelDelta = 0.0f;
}

void InputState::setKeyState(Key key, bool isDown) {
    const auto idx = static_cast<std::size_t>(key);
    if (idx < kKeyCount) {
        m_currentKeys[idx] = isDown;
    }
}

void InputState::setMouseButtonState(MouseButton button, bool isDown) {
    const auto idx = static_cast<std::size_t>(button);
    if (idx < kMouseButtonCount) {
        m_currentMouseButtons[idx] = isDown;
    }
}

void InputState::addMouseDelta(float dx, float dy) {
    m_mouseDeltaX += dx;
    m_mouseDeltaY += dy;
}

void InputState::setMousePosition(float x, float y) {
    m_mouseX = x;
    m_mouseY = y;
}

void InputState::addMouseWheel(float dy) {
    m_mouseWheelDelta += dy;
}

bool InputState::isKeyDown(Key key) const {
    const auto idx = static_cast<std::size_t>(key);
    return idx < kKeyCount ? m_currentKeys[idx] : false;
}

bool InputState::wasKeyPressed(Key key) const {
    const auto idx = static_cast<std::size_t>(key);
    return idx < kKeyCount ? (m_currentKeys[idx] && !m_previousKeys[idx]) : false;
}

bool InputState::wasKeyReleased(Key key) const {
    const auto idx = static_cast<std::size_t>(key);
    return idx < kKeyCount ? (!m_currentKeys[idx] && m_previousKeys[idx]) : false;
}

bool InputState::isMouseButtonDown(MouseButton button) const {
    const auto idx = static_cast<std::size_t>(button);
    return idx < kMouseButtonCount ? m_currentMouseButtons[idx] : false;
}

bool InputState::wasMouseButtonPressed(MouseButton button) const {
    const auto idx = static_cast<std::size_t>(button);
    return idx < kMouseButtonCount ? (m_currentMouseButtons[idx] && !m_previousMouseButtons[idx]) : false;
}

} // namespace sage
