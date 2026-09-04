#ifndef GALLIUM__CORE__UIOBJECT_H
#define GALLIUM__CORE__UIOBJECT_H
#pragma once

namespace ga::core
{
	class UiObject
	{
	public:
		virtual void RenderUi() = 0;
	};
}

#endif /* GALLIUM__CORE__UIOBJECT_H */
