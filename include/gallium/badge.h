#ifndef GALLIUM__BADGE_H
#define GALLIUM__BADGE_H
#pragma once

namespace ga
{
	template<typename T>
	class Badge { friend T; Badge() {} Badge(const Badge&) {} };
}

#endif /* GALLIUM__BADGE_H */
