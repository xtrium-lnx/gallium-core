#include <gallium/core/inputtranslator.h>
#include <gallium/core/messagebus.h>

#include <GLFW/glfw3.h>

using namespace ga::core;

struct InputTranslator::Internal
{
	EInputTranslationMode translationMode;
	InputMouseState       mouseState;

	Window*               window;
	
	MessageHandler*       onRawKeyboard = nullptr;
	MessageHandler*       onRawMousePos = nullptr;
	MessageHandler*       onRawMouseBtn = nullptr;
	MessageHandler*       onRawScroll   = nullptr;

	std::unordered_map<string_hash_t, bool> keysDown;
};

void InputTranslator::OnRawKeyboard(MessageDataBase* msg)
{
	if (m_pImpl->translationMode != EInputTranslationMode::Normal)
		return;

	InputKeyboardRawData* data = CTTI_CAST(msg, InputKeyboardRawData);
	assert(data && "Message data is null");

	static auto keycodeToString = [](uint32_t keycode) -> std::string
	{
		if (keycode >= GLFW_KEY_A && keycode <= GLFW_KEY_Z)
			return std::string(1, 'A' + (keycode - GLFW_KEY_A));

		if (keycode >= GLFW_KEY_0 && keycode <= GLFW_KEY_9)
			return std::string(1, '0' + (keycode - GLFW_KEY_0));

		if (keycode >= GLFW_KEY_KP_0 && keycode <= GLFW_KEY_KP_9)
			return "Numpad" + std::string(1, '0' + (keycode - GLFW_KEY_KP_0));

		if (keycode >= GLFW_KEY_F1 && keycode <= GLFW_KEY_F9)
			return "F" + std::string(1, '1' + (keycode - GLFW_KEY_F1));

		if (keycode >= GLFW_KEY_F10 && keycode <= GLFW_KEY_F12)
			return "F1" + std::string(1, '0' + (keycode - GLFW_KEY_F10));

		switch (keycode)
		{
		case GLFW_KEY_SPACE:         return "Space";
		case GLFW_KEY_ESCAPE:        return "Escape";
		case GLFW_KEY_ENTER:         return "Enter";
		case GLFW_KEY_TAB:           return "Tab";
		case GLFW_KEY_BACKSPACE:     return "Backspace";
		case GLFW_KEY_INSERT:        return "Ins";
		case GLFW_KEY_DELETE:        return "Del";
		case GLFW_KEY_RIGHT:         return "Right";
		case GLFW_KEY_LEFT:          return "Left";
		case GLFW_KEY_DOWN:          return "Down";
		case GLFW_KEY_UP:            return "Up";
		case GLFW_KEY_PAGE_UP:       return "PgUp";
		case GLFW_KEY_PAGE_DOWN:     return "PgDn";
		case GLFW_KEY_HOME:          return "Home";
		case GLFW_KEY_END:           return "End";
		case GLFW_KEY_PAUSE:         return "Pause";

		case GLFW_KEY_APOSTROPHE:    return "'";
		case GLFW_KEY_COMMA:         return ",";
		case GLFW_KEY_SEMICOLON:     return ";";
		case GLFW_KEY_PERIOD:
		case GLFW_KEY_KP_DECIMAL:    return ".";
		case GLFW_KEY_KP_SUBTRACT:
		case GLFW_KEY_MINUS:         return "-";
		case GLFW_KEY_KP_ADD:        return "+";
		case GLFW_KEY_KP_MULTIPLY:   return "*";
		case GLFW_KEY_EQUAL:         return "=";
		case GLFW_KEY_KP_DIVIDE:
		case GLFW_KEY_SLASH:         return "/";
		case GLFW_KEY_BACKSLASH:     return "\\";
		case GLFW_KEY_LEFT_BRACKET:  return "[";
		case GLFW_KEY_RIGHT_BRACKET: return "]";
		case GLFW_KEY_GRAVE_ACCENT:  return "`";
		case GLFW_KEY_LEFT_SHIFT:    return "LShift";
		case GLFW_KEY_RIGHT_SHIFT:   return "RShift";
		case GLFW_KEY_LEFT_CONTROL:  return "LCtrl";
		case GLFW_KEY_RIGHT_CONTROL: return "RCtrl";
		case GLFW_KEY_LEFT_ALT:      return "LAlt";
		case GLFW_KEY_RIGHT_ALT:     return "RAlt";
		}

		return "Unknown";
	};

	std::string keyCombination = "input:key_" + std::string(data->data.pressed ? "pressed:" : "released:");

	if (data->data.modifiers & EInputKeyboardModifiers::Control && ((data->data.keycode) != GLFW_KEY_LEFT_CONTROL && (data->data.keycode) != GLFW_KEY_RIGHT_CONTROL))
		keyCombination += "Ctrl+";
	if (data->data.modifiers & EInputKeyboardModifiers::Alt && ((data->data.keycode) != GLFW_KEY_LEFT_ALT && (data->data.keycode) != GLFW_KEY_RIGHT_ALT))
		keyCombination += "Alt+";
	if (data->data.modifiers & EInputKeyboardModifiers::Shift && ((data->data.keycode) != GLFW_KEY_LEFT_SHIFT && (data->data.keycode) != GLFW_KEY_RIGHT_SHIFT))
		keyCombination += "Shift+";
	keyCombination += keycodeToString(data->data.keycode);

	string_hash_t kHash = hash_string(keycodeToString(data->data.keycode));
	m_pImpl->keysDown[kHash]    = data->data.pressed;

	MessageBus::Global().Post(hash_string(keyCombination));
}

void InputTranslator::OnRawText(MessageDataBase* msg)
{
	if (m_pImpl->translationMode != EInputTranslationMode::Text)
		return;

	InputTextRawData* data = CTTI_CAST(msg, InputTextRawData);
	assert(data && "Message data is null");

	MessageBus::Global().Post("input:text"_h, InputTextData({ char(data->data.codepoint) }));
}

void InputTranslator::OnRawMousePosition(MessageDataBase* msg)
{
	InputMousePositionRawData* data = CTTI_CAST(msg, InputMousePositionRawData);
	assert(data && "Message data is null");

	m_pImpl->mouseState.position = data->data.position;
	MessageBus::Global().Post("input:mouseposition"_h, InputMousePositionData({ m_pImpl->mouseState.position }));
}

void InputTranslator::OnRawMouseButton(MessageDataBase* msg)
{
	InputMouseButtonRawData* data = CTTI_CAST(msg, InputMouseButtonRawData);
	assert(data && "Message data is null");

	if (data->data.pressed)
		m_pImpl->mouseState.buttonsPressed |= (1 << data->data.button);
	else
		m_pImpl->mouseState.buttonsPressed &= ~(1 << data->data.button);

	MessageBus::Global().Post("input:mousebutton"_h, InputMouseButtonData({ EInputMouseButtons::Enum(1 << data->data.button), data->data.pressed }));
}

void InputTranslator::OnRawScroll(MessageDataBase* msg)
{
	InputScrollRawData* data = CTTI_CAST(msg, InputScrollRawData);
	assert(data && "Message data is null");

	int32_t ofs = int32_t(data->data.scrollOffset.y);

	// [xtrium] FIXME: Maybe ofs isn't like 1 for one line (I remember smth around 120 for one line)
	//                 Once we actually use this, fix amount accordingly.
	MessageBus::Global().Post("input:scroll"_h, InputScrollData({ ofs > 0 ? EInputScrollDirection::Up : EInputScrollDirection::Down, uint32_t(ofs > 0 ? ofs : -ofs) }));
}

InputTranslator::InputTranslator()
	: m_pImpl(new Internal)
{
}

InputTranslator::~InputTranslator() noexcept
{
}

void InputTranslator::Initialize()
{
	m_pImpl->translationMode = EInputTranslationMode::Normal;
	m_pImpl->window = nullptr;

	m_pImpl->onRawKeyboard = new ObjectMemberDelegate(this, &InputTranslator::OnRawKeyboard);
	m_pImpl->onRawMousePos = new ObjectMemberDelegate(this, &InputTranslator::OnRawMousePosition);
	m_pImpl->onRawMouseBtn = new ObjectMemberDelegate(this, &InputTranslator::OnRawMouseButton);
	m_pImpl->onRawScroll   = new ObjectMemberDelegate(this, &InputTranslator::OnRawScroll);

	MessageBus::Global().AddRecipient("input:keyboard_raw"_h,       m_pImpl->onRawKeyboard);
	MessageBus::Global().AddRecipient("input:mouse_position_raw"_h, m_pImpl->onRawMousePos);
	MessageBus::Global().AddRecipient("input:mouse_button_raw"_h,   m_pImpl->onRawMouseBtn);
	MessageBus::Global().AddRecipient("input:scroll_raw"_h,         m_pImpl->onRawScroll);
}

void InputTranslator::Terminate()
{
	if (m_pImpl->onRawKeyboard)
	{
		MessageBus::Global().RemoveRecipient("input:keyboard_raw"_h,       m_pImpl->onRawKeyboard);
		MessageBus::Global().RemoveRecipient("input:mouse_position_raw"_h, m_pImpl->onRawMousePos);
		MessageBus::Global().RemoveRecipient("input:mouse_button_raw"_h,   m_pImpl->onRawMouseBtn);
		MessageBus::Global().RemoveRecipient("input:scroll_raw"_h,         m_pImpl->onRawScroll);
	}

	GA_SAFE_DELETE(m_pImpl->onRawKeyboard);
	GA_SAFE_DELETE(m_pImpl->onRawMousePos);
	GA_SAFE_DELETE(m_pImpl->onRawMouseBtn);
	GA_SAFE_DELETE(m_pImpl->onRawScroll);
}

bool InputTranslator::IsKeyDown(string_hash_t kHash) const
{
	if (!m_pImpl->keysDown.count(kHash))
		return false;

	return m_pImpl->keysDown[kHash];
}

bool InputTranslator::IsMouseButtonDown(EInputMouseButtons::Enum mouseButton) const
{
	return m_pImpl->mouseState.buttonsPressed & mouseButton;
}

glm::vec2 InputTranslator::MousePosition() const
{
	return m_pImpl->mouseState.position;
}

EInputTranslationMode InputTranslator::GetMode() const
{
	return m_pImpl->translationMode;
}

void InputTranslator::SetMode(EInputTranslationMode mode)
{
	m_pImpl->translationMode = mode;
}

void InputTranslator::SetReferenceWindow(Window* window)
{
	m_pImpl->window = window;
}
