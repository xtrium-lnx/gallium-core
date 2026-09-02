#include "input_impl.h"
#include "platform_impl.h"

#include <gallium/overloaded.h>
#include <cassert>

using namespace ga::platform;

static Input::VariableKey s_ParseKey(const std::string& name)
{
    // Modifiers
    if (name == "Ctrl")                      return Input::MultiKey { EKey::LeftControl, EKey::RightControl };
    if (name == "LCtrl")                     return EKey::RightControl;
    if (name == "RCtrl")                     return EKey::RightControl;
    if (name == "Shift")                     return Input::MultiKey { EKey::LeftShift, EKey::RightShift };
    if (name == "LShift")                    return EKey::LeftShift;
    if (name == "RShift")                    return EKey::RightShift;
    if (name == "Alt")                       return Input::MultiKey { EKey::LeftAlt, EKey::RightAlt };
    if (name == "LAlt")                      return EKey::LeftAlt;
    if (name == "RAlt")                      return EKey::RightAlt;

    // Special keys
    if (name == "Space")       return EKey::Space;
    if (name == "Enter")       return EKey::Enter;
    if (name == "Escape")      return EKey::Escape;
    if (name == "Tab")         return EKey::Tab;
    if (name == "Backspace")   return EKey::Backspace;
    if (name == "Insert")      return EKey::Insert;
    if (name == "Delete")      return EKey::Delete;
    if (name == "Right")       return EKey::Right;
    if (name == "Left")        return EKey::Left;
    if (name == "Up")          return EKey::Up;
    if (name == "Down")        return EKey::Down;
    if (name == "PageUp")      return EKey::PageUp;
    if (name == "PageDown")    return EKey::PageDown;
    if (name == "Home")        return EKey::Home;
    if (name == "End")         return EKey::End;
    if (name == "CapsLock")    return EKey::CapsLock;
    if (name == "ScrollLock")  return EKey::ScrollLock;
    if (name == "NumLock")     return EKey::NumLock;
    if (name == "PrintScreen") return EKey::PrintScreen;
    if (name == "Pause")       return EKey::Pause;

    // Function keys F1–F12
    if (name.size() >= 2 && name[0] == 'F')
    {
        int n = std::stoi(name.substr(1));
        if (n >= 1 && n <= 12)
            return static_cast<EKey>(static_cast<int>(EKey::F1) + (n - 1));
    }

    // Single letter A–Z
    if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z')
        return static_cast<EKey>(name[0]); // EKey::A == 65 == 'A'

    // Single digit 0–9
    if (name.size() == 1 && name[0] >= '0' && name[0] <= '9')
        return static_cast<EKey>(name[0]); // EKey::Num0 == 48 == '0'

    // Numpad
    if (name.size() >= 7 && name.substr(0, 6) == "Numpad")
    {
        std::string sub = name.substr(6);
        if (sub.size() == 1 && sub[0] >= '0' && sub[0] <= '9')
            return static_cast<EKey>(static_cast<int>(EKey::Numpad0) + (sub[0] - '0'));
        if (sub == "Decimal")  return EKey::NumpadDecimal;
        if (sub == "Divide")   return EKey::NumpadDivide;
        if (sub == "Multiply") return EKey::NumpadMultiply;
        if (sub == "Subtract") return EKey::NumpadSubtract;
        if (sub == "Add")      return EKey::NumpadAdd;
        if (sub == "Enter")    return EKey::NumpadEnter;
        if (sub == "Equal")    return EKey::NumpadEqual;
    }

    // Punctuation by character
    if (name == "'")  return EKey::Apostrophe;
    if (name == ",")  return EKey::Comma;
    if (name == "-")  return EKey::Minus;
    if (name == ".")  return EKey::Period;
    if (name == "/")  return EKey::Slash;
    if (name == ";")  return EKey::Semicolon;
    if (name == "=")  return EKey::Equal;
    if (name == "[")  return EKey::LeftBracket;
    if (name == "\\") return EKey::Backslash;
    if (name == "]")  return EKey::RightBracket;
    if (name == "`")  return EKey::GraveAccent;

    // Unknown — assert/log in debug, return a safe fallback
    assert(false && "s_ParseKey: unrecognised key name");
    std::unreachable();
}

void Input::Impl::Init(GLFWwindow* window)
{
    this->window = window;

    glfwSetKeyCallback(window, Impl::KeyCallback);
    glfwSetMouseButtonCallback(window, Impl::MouseButtonCallback);
    glfwSetScrollCallback(window, Impl::ScrollCallback);
    glfwSetCharCallback(window, Impl::CharCallback);
}

void Input::Impl::PollEvents()
{
    for (auto& k : keys)
    {
        k.pressed  = false;
        k.released = false;
    }
    for (auto& b : mouseButtons)
    {
        b.pressed  = false;
        b.released = false;
    }

    scrollDelta = { 0.f, 0.f };

    inputChars = pendingChars;
    pendingChars.clear();

    for (auto& [glfwKey, action] : pendingKeyEvents)
    {
        if (glfwKey < 0 || glfwKey >= int(EKey::Count))
            continue;

        auto& state = keys[glfwKey];

        if (action == GLFW_PRESS)
        {
            state.held    = true;
            state.pressed = true;

            for (auto& cb : keyCallbacks[glfwKey].onPressed)
                cb();
        }
        else if (action == GLFW_RELEASE)
        {
            state.held     = false;
            state.released = true;

            for (auto& cb : keyCallbacks[glfwKey].onReleased)
                cb();
        }
    }
    pendingKeyEvents.clear();

    for (uint32_t i = 0; i < uint32_t(EKey::Count); ++i)
        if (keys[i].held)
            for (auto& cb : keyCallbacks[i].onHeld)
                cb();

    for (auto& [button, action] : pendingMouseButtonEvents)
    {
        if (button < 0 || button >= int(EMouseButton::Count))
            continue;

        auto& state = mouseButtons[button];

        if (action == GLFW_PRESS)
        {
            state.held    = true;
            state.pressed = true;
            for (auto& cb : mouseButtonCallbacks[button].onPressed)
                cb();
        }
        else if (action == GLFW_RELEASE)
        {
            state.held     = false;
            state.released = true;
            for (auto& cb : mouseButtonCallbacks[button].onReleased)
                cb();
        }
    }
    pendingMouseButtonEvents.clear();

    mousePrevPosition = mousePosition;

    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    mousePosition = { float(mx), float(my) };
    mouseDelta    = mousePosition - mousePrevPosition;

    scrollDelta   = pendingScroll;
    pendingScroll = { 0.f, 0.f };
}

Input::Input(const ga::platform::Platform& platform)
    : m_pImpl(new Impl)
{
    m_pImpl->Init(platform.GetImpl().surface.window);
}

Input::Impl& Input::GetImpl() const
{
    return *m_pImpl;
}

bool Input::IsPressed(EKey key) const
{
    return m_pImpl->keys[size_t(key)].pressed;
}

bool Input::IsHeld(EKey key) const
{
    return m_pImpl->keys[size_t(key)].held;
}

bool Input::IsReleased(EKey key) const
{
    return m_pImpl->keys[size_t(key)].released;
}

bool Input::IsPressed(EMouseButton b) const
{
    return m_pImpl->mouseButtons[size_t(b)].pressed;
}

bool Input::IsHeld(EMouseButton b) const
{
    return m_pImpl->mouseButtons[size_t(b)].held;
}

bool Input::IsReleased(EMouseButton b) const
{
    return m_pImpl->mouseButtons[size_t(b)].released;
}

Input::KeyCombo Input::CreateKeyCombo(const std::string& combo)
{
    KeyCombo result;

    size_t start = 0, pos = 0;
    while ((pos = combo.find('+', start)) != std::string::npos)
    {
        result.push_back(s_ParseKey(combo.substr(start, pos - start)));
        start = pos + 1;
    }
    result.push_back(s_ParseKey(combo.substr(start)));

    return result;
}

bool Input::IsKeyComboPressed(const KeyCombo& combo)
{
    bool result = true;

    for (size_t i = 0; i < combo.size(); ++i)
    {
        bool isLast = (i == combo.size() - 1);
        const VariableKey& vk = combo[i];

        std::visit(ga::overloaded {
            [&](const std::vector<EKey>& multi) {
                bool subresult = false;
                for (auto& k : multi)
                {
                    if (isLast)
                        subresult = subresult || IsPressed(k);
                    else
                        subresult = subresult || IsHeld(k);

                    result = result && subresult;
                }
            },
            [&](const EKey& k) {
                if (isLast)
                    result = result && IsPressed(k);
                else
                    result = result && IsHeld(k);
            }
        }, vk);
    }

    return result;
}

bool Input::IsKeyComboPressed(const std::string& combo)
{
    return IsKeyComboPressed(CreateKeyCombo(combo));
}

glm::vec2 Input::MousePosition() const
{
    return m_pImpl->mousePosition;
}

glm::vec2 Input::MouseDelta() const
{
    return m_pImpl->mouseDelta;
}

glm::vec2 Input::ScrollDelta() const
{
    return m_pImpl->scrollDelta;
}

void Input::SetMouseMode(EMouseMode mode)
{
    m_pImpl->mouseMode = mode;
    glfwSetInputMode(m_pImpl->window, GLFW_CURSOR, mode == EMouseMode::Captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    // Reset delta on mode switch to avoid a jump
    double mx, my;
    glfwGetCursorPos(m_pImpl->window, &mx, &my);
    m_pImpl->mousePosition     = { float(mx), float(my) };
    m_pImpl->mousePrevPosition = m_pImpl->mousePosition;
    m_pImpl->mouseDelta        = { 0.f, 0.f };
}

EMouseMode Input::MouseMode() const
{
    return m_pImpl->mouseMode;
}

const std::vector<uint32_t> Input::InputChars() const
{
    return m_pImpl->inputChars;
}

void Input::OnPressed(EKey key, std::function<void()> cb)
{
    m_pImpl->keyCallbacks[uint32_t(key)].onPressed.push_back(std::move(cb));
}

void Input::OnHeld(EKey key, std::function<void()> cb)
{
    m_pImpl->keyCallbacks[uint32_t(key)].onHeld.push_back(std::move(cb));
}

void Input::OnReleased(EKey key, std::function<void()> cb)
{
    m_pImpl->keyCallbacks[uint32_t(key)].onReleased.push_back(std::move(cb));
}

void Input::OnPressed(EMouseButton btn, std::function<void()> cb)
{
    m_pImpl->mouseButtonCallbacks[uint32_t(btn)].onPressed.push_back(std::move(cb));
}

void Input::OnReleased(EMouseButton btn, std::function<void()> cb)
{
    m_pImpl->mouseButtonCallbacks[uint32_t(btn)].onReleased.push_back(std::move(cb));
}
