#ifndef GALLIUM__CORE__INPUTMESSAGES_H
#define GALLIUM__CORE__INPUTMESSAGES_H
#pragma once

#include <gallium/core/messagebus.h>
#include <glm/glm.hpp>

namespace ga::core
{
	namespace EInputKeyboardModifiers {
		enum Enum
		{
			Shift   = 0x1,
			Control = 0x2,
			Alt     = 0x4
		};
	}

	GA_MESSAGEDATA(InputKeyboardRawData,
		int32_t  keycode;
		uint32_t modifiers;
		bool     pressed;
	);

	GA_MESSAGEDATA(InputTextRawData,
		int32_t codepoint;
	);

	GA_MESSAGEDATA(InputMousePositionRawData,
		glm::vec2 position;
	);

	GA_MESSAGEDATA(InputMouseButtonRawData,
		uint32_t button;
		bool     pressed;
	);

	GA_MESSAGEDATA(InputScrollRawData,
		glm::vec2 scrollOffset;
	);
}

#endif /* GALLIUM__CORE__INPUTMESSAGES_H */