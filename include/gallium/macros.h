#ifndef GALLIUM__MACROS_H
#define GALLIUM__MACROS_H
#pragma once

#ifndef NDEBUG
# define GA_DEBUG_ONLY(X) X
#else
# define GA_DEBUG_ONLY(X)
#endif

#define GA_SAFE_DELETE(X) { delete X; X = nullptr; }

#endif /* GALLIUM__MACROS_H */
