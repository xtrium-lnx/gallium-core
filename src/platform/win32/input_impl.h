#ifndef GALLIUM__PLATFORM__WIN32__INPUT_H
#define GALLIUM__PLATFORM__WIN32__INPUT_H
#pragma once

#include <array>
#include <functional>
#include <vector>

#include <GLFW/glfw3.h>

#include <gallium/platform/platform.h>
#include <gallium/platform/input.h>

namespace ga::platform
{
    struct ButtonState
    {
        bool held     = false;
        bool pressed  = false; // true for one frame only
        bool released = false; // true for one frame only
    };

    struct KeyCallbacks
    {
        std::vector<std::function<void()>> onPressed;
        std::vector<std::function<void()>> onHeld;
        std::vector<std::function<void()>> onReleased;
    };

    struct MouseButtonCallbacks
    {
        std::vector<std::function<void()>> onPressed;
        std::vector<std::function<void()>> onReleased;
    };

    struct Input::Impl
    {
        GLFWwindow* window = nullptr;

        std::array<ButtonState, size_t(EKey::Count)>                  keys;
        std::array<KeyCallbacks, size_t(EKey::Count)>                 keyCallbacks;
        std::vector<std::pair<int /* glfwKey */, int /* action */>>   pendingKeyEvents;

        std::array<ButtonState, size_t(EMouseButton::Count)>          mouseButtons;
        std::array<MouseButtonCallbacks, size_t(EMouseButton::Count)> mouseButtonCallbacks;
        std::vector<std::pair<int /* button */, int /* action */>>    pendingMouseButtonEvents;

        glm::vec2  mousePosition     = { 0.f, 0.f };
        glm::vec2  mousePrevPosition = { 0.f, 0.f };
        glm::vec2  mouseDelta        = { 0.f, 0.f };
        glm::vec2  scrollDelta       = { 0.f, 0.f };
        glm::vec2  pendingScroll     = { 0.f, 0.f };
        EMouseMode mouseMode         = EMouseMode::Free;

        std::vector<uint32_t> pendingChars;
        std::vector<uint32_t> inputChars;

        void Init(GLFWwindow* window);
        void PollEvents();

        static void KeyCallback(GLFWwindow* w, int key, int scancode, int action, int mods)
        {
            auto& self = static_cast<Platform*>(glfwGetWindowUserPointer(w))->Input().GetImpl();
            if (action != GLFW_REPEAT)
                self.pendingKeyEvents.emplace_back(key, action);
        }

        static void MouseButtonCallback(GLFWwindow* w, int button, int action, int mods)
        {
            auto& self = static_cast<Platform*>(glfwGetWindowUserPointer(w))->Input().GetImpl();
            self.pendingMouseButtonEvents.emplace_back(button, action);
        }

        static void ScrollCallback(GLFWwindow* w, double x, double y)
        {
            auto& self = static_cast<Platform*>(glfwGetWindowUserPointer(w))->Input().GetImpl();
            self.pendingScroll += glm::vec2(float(x), float(y));
        }

        static void CharCallback(GLFWwindow* w, unsigned int codepoint)
        {
            auto& self = static_cast<Platform*>(glfwGetWindowUserPointer(w))->Input().GetImpl();
            self.pendingChars.push_back(codepoint);
        }
    };
}

#endif /* GALLIUM__PLATFORM__WIN32__INPUT_H */
