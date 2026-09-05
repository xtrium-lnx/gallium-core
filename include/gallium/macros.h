#ifndef GALLIUM__MACROS_H
#define GALLIUM__MACROS_H
#pragma once

#ifdef _WIN32
# define NOMINMAX
# define WIN32_LEAN_AND_MEAN
# define WIN32_EXTRA_LEAN
# include <Windows.h>
# undef CreateFile
# undef GetObject
# undef NOMINMAX
#endif /* _WIN32 */

#define GA_SAFE_DELETE(X) { delete X; X = nullptr; }

#define GA_STRINGIZE(x) GA_STRINGIZE2(x)
#define GA_STRINGIZE2(x) #x
#define GA_LINE_STRING GA_STRINGIZE(__LINE__)

#ifndef NDEBUG
# define GA_NOT_IMPLEMENTED() {__debugbreak();}
# define GA_DEBUG_ONLY(X) X
# define GA_ASSERT(X) { if (!(X)) __debugbreak(); }
# ifdef _WIN32
#  define GA_ASSERT_MSG(X, MSG) { if (!(X)) { OutputDebugStringA("ASSERTION FAILED at " __FILE__ ":" GA_LINE_STRING); OutputDebugStringA(#X); OutputDebugStringA(MSG); __debugbreak(); } }
# else /* _WIN32  */
#  define GA_ASSERT_MSG(X, MSG) { if (!(X)) { std::cout << "ASSERTION FAILED at " __FILE__ ":" GA_LINE_STRING << std::endl << (#X) << MSG << std::endl; __debugbreak(); } }
# endif /* _WIN32 */
#else // !NDEBUG
# define GA_NOT_IMPLEMENTED()
# define GA_DEBUG_ONLY(X)
# define GA_ASSERT(X) X
# define GA_ASSERT_MSG(X, MSG_FUNC, ...) X
#endif // !NDEBUG

#endif /* GALLIUM__MACROS_H */
