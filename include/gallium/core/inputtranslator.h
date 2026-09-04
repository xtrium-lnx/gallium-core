#ifndef GALLIUM__CORE__INPUTTRANSLATOR_H
#define GALLIUM__CORE__INPUTTRANSLATOR_H
#pragma once

#include <gallium/string_hash.h>
#include <gallium/core/inputmessages.h>

#include <glm/glm.hpp>

#include <memory>
#include <string>

namespace ga::core
{
	class Window;

	enum class EInputTranslationMode
	{
		Normal,
		Text
	};

	enum class EInputKeyEventType
	{
		Pressed,
		Released
	};

	namespace EInputMouseButtons { enum Enum
	{
		Left   = 0x1,
		Middle = 0x4,
		Right  = 0x2
	}; }

	enum class EInputScrollDirection
	{
		Up,
		Down
	};

	struct InputMouseState
	{
		glm::vec2 position = glm::vec2(0);
		uint32_t  buttonsPressed = 0;
	};

	GA_MESSAGEDATA(InputKeyData,
		std::string        keyCombination;
		EInputKeyEventType state;
	);

	GA_MESSAGEDATA(InputTextData,
		char character;
	);

	GA_MESSAGEDATA(InputMousePositionData,
		glm::vec2 position;
	);

	GA_MESSAGEDATA(InputMouseButtonData,
		EInputMouseButtons::Enum button;
		bool                     pressed;
	);

	GA_MESSAGEDATA(InputScrollData,
		EInputScrollDirection direction;
		uint32_t              amount;
	);

	class InputTranslator
		: public GlobalInstance<InputTranslator>
	{
		struct Internal;
		std::unique_ptr<Internal> m_pImpl;

		void OnRawKeyboard(MessageDataBase* msg);
		void OnRawText(MessageDataBase* msg);
		void OnRawMousePosition(MessageDataBase* msg);
		void OnRawMouseButton(MessageDataBase* msg);
		void OnRawScroll(MessageDataBase* msg);

	public:
		explicit InputTranslator();
		~InputTranslator() noexcept;

		void Initialize() override;
		void Terminate() override;

		bool IsKeyDown(string_hash_t kHash) const;
		bool IsMouseButtonDown(EInputMouseButtons::Enum mouseButton) const;
		glm::vec2 MousePosition() const;

		EInputTranslationMode GetMode() const;
		void SetMode(EInputTranslationMode mode);

		void SetReferenceWindow(Window* window);
	};
}

#define GA_ON_KEY_PRESSED(K, LAMBDA) \
	ga::core::MessageBus::Global().AddRecipient(ga::hash_string("input:key_pressed:" ## K), new ga::core::LambdaDelegate<void, ga::core::MessageDataBase*>(LAMBDA))

#define GA_ON_KEY_RELEASED(K, LAMBDA) \
	ga::core::MessageBus::Global().AddRecipient(ga::hash_string("input:key_released:" ## K), new ga::core::LambdaDelegate<void, ga::core::MessageDataBase*>(LAMBDA))

#endif /* GALLIUM__CORE__INPUTTRANSLATOR_H */
