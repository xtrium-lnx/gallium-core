#include <gallium/core/gamepad.h>

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <Windows.h>

#include <Xinput.h>
#pragma comment(lib, "xinput.lib")

using namespace ga::core;

WORD s_ToXInput(EGamepadButton button)
{
	switch (button)
	{
	case EGamepadButton::A:          return XINPUT_GAMEPAD_A;
	case EGamepadButton::B:          return XINPUT_GAMEPAD_B;
	case EGamepadButton::X:          return XINPUT_GAMEPAD_X;
	case EGamepadButton::Y:          return XINPUT_GAMEPAD_Y;
	case EGamepadButton::LThumb:     return XINPUT_GAMEPAD_LEFT_THUMB;
	case EGamepadButton::RThumb:     return XINPUT_GAMEPAD_RIGHT_THUMB;
	case EGamepadButton::LShoulder:  return XINPUT_GAMEPAD_LEFT_SHOULDER;
	case EGamepadButton::RShoulder:  return XINPUT_GAMEPAD_RIGHT_SHOULDER;
	case EGamepadButton::DPad_Left:  return XINPUT_GAMEPAD_DPAD_LEFT;
	case EGamepadButton::DPad_Right: return XINPUT_GAMEPAD_DPAD_RIGHT;
	case EGamepadButton::DPad_Up:    return XINPUT_GAMEPAD_DPAD_UP;
	case EGamepadButton::DPad_Down:  return XINPUT_GAMEPAD_DPAD_DOWN;
	case EGamepadButton::Start:      return XINPUT_GAMEPAD_START;
	case EGamepadButton::Back:       return XINPUT_GAMEPAD_BACK;
	}

	return 0;
}

struct Gamepad::Internal
{
	int cId;
	XINPUT_STATE state;

	float deadzoneX;
	float deadzoneY;
};

Gamepad::Gamepad(float dzX, float dzY)
	: m_pImpl(new Internal)
{
	m_pImpl->deadzoneX = dzX;
	m_pImpl->deadzoneY = dzY;
}

Gamepad::Gamepad()
	: Gamepad(0.02f, 0.02f)
{
}

Gamepad::~Gamepad() noexcept
{
}

int Gamepad::GetPort()
{
	return m_pImpl->cId + 1;
}

bool Gamepad::CheckConnection()
{
	int controllerId = -1;

	for (DWORD i = 0; i < XUSER_MAX_COUNT && controllerId == -1; i++)
	{
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));

		if (XInputGetState(i, &state) == ERROR_SUCCESS)
			controllerId = i;
	}

	m_pImpl->cId = controllerId;

	return controllerId != -1;
}

void Gamepad::Update()
{
	if (m_pImpl->cId == -1)
		CheckConnection();

	if (m_pImpl->cId != -1)
	{
		ZeroMemory(&m_pImpl->state, sizeof(XINPUT_STATE));
		if (XInputGetState(m_pImpl->cId, &m_pImpl->state) != ERROR_SUCCESS)
		{
			m_pImpl->cId = -1;
			return;
		}

		float normLX = fmaxf(-1, (float)m_pImpl->state.Gamepad.sThumbLX / 32767);
		float normLY = fmaxf(-1, (float)m_pImpl->state.Gamepad.sThumbLY / 32767);

		leftStickX = (abs(normLX) < m_pImpl->deadzoneX ? 0 : (abs(normLX) - m_pImpl->deadzoneX) * (normLX / abs(normLX)));
		leftStickY = (abs(normLY) < m_pImpl->deadzoneY ? 0 : (abs(normLY) - m_pImpl->deadzoneY) * (normLY / abs(normLY)));

		if (m_pImpl->deadzoneX > 0) leftStickX *= 1 / (1 - m_pImpl->deadzoneX);
		if (m_pImpl->deadzoneY > 0) leftStickY *= 1 / (1 - m_pImpl->deadzoneY);

		float normRX = fmaxf(-1, (float)m_pImpl->state.Gamepad.sThumbRX / 32767);
		float normRY = fmaxf(-1, (float)m_pImpl->state.Gamepad.sThumbRY / 32767);

		rightStickX = (abs(normRX) < m_pImpl->deadzoneX ? 0 : (abs(normRX) - m_pImpl->deadzoneX) * (normRX / abs(normRX)));
		rightStickY = (abs(normRY) < m_pImpl->deadzoneY ? 0 : (abs(normRY) - m_pImpl->deadzoneY) * (normRY / abs(normRY)));

		if (m_pImpl->deadzoneX > 0) rightStickX *= 1 / (1 - m_pImpl->deadzoneX);
		if (m_pImpl->deadzoneY > 0) rightStickY *= 1 / (1 - m_pImpl->deadzoneY);

		leftTrigger  = (float)m_pImpl->state.Gamepad.bLeftTrigger / 255;
		rightTrigger = (float)m_pImpl->state.Gamepad.bRightTrigger / 255;
	}
}

bool Gamepad::IsButtonPressed(EGamepadButton button)
{
	return (m_pImpl->state.Gamepad.wButtons & s_ToXInput(button)) != 0;
}
