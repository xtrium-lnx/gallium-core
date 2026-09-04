#ifndef GALLIUM__CORE__GAMEPAD_H
#define GALLIUM__CORE__GAMEPAD_H

#include <gallium/globalinstance.h>
#include <memory>

namespace ga::core
{
	enum class EGamepadButton
	{
		A, B, X, Y,

		LThumb, RThumb,
		LShoulder, RShoulder,

		DPad_Left, DPad_Right, DPad_Up, DPad_Down,

		Start, Back
	};

	class Gamepad
		: public GlobalInstance<Gamepad>
	{
	private:
		struct Internal;
		std::unique_ptr<Internal> m_pImpl;

	public:
		float leftStickX;
		float leftStickY;
		float rightStickX;
		float rightStickY;
		float leftTrigger;
		float rightTrigger;

		Gamepad(float dzX, float dzY);
		Gamepad();
		~Gamepad() noexcept;

		int  GetPort();
		bool CheckConnection();
		void Update();
		bool IsButtonPressed(EGamepadButton button);
	};
}

#endif /* GALLIUM__CORE__GAMEPAD_H */
