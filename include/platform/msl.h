// MSL (CodeWarrior's C/C++ runtime) behaviour that game code depends on and that the host
// C library does not reproduce. Declarations only; the platform layer implements them.
#pragma once
#include <cstdint>

// CodeWarrior's assert() calls MSL's __assertion_failed(condition, file, line), which
// prints "Assertion (cond) failed in "file", line n" to stderr and calls abort(). It is an
// ordinary call for the compiler, so the code after a failed assert exists; translations
// call this with the original's exact arguments (condition text, original file name and
// line), and the platform layer prints and aborts as MSL did. Tests record the call and
// return, so both sides' asserts can be compared.
void msl_assertion_failed(const char* condition, const char* file, int32_t line);

// MSL's rand and srand (c_c++ 0x10027ca4 and 0x10027ce4), one generator shared by the game
// and its libraries: state = state * 0x41c64e6d + 0x3039, result (state >> 16) & 0x7fff;
// the state starts at 1. The game seeds it once from the clock (GAMESYSTEM_Init) and the
// engine fills its weather and sky tables from it, so translations call these and not
// the host library's.
int32_t msl_rand();
void msl_srand(uint32_t seed);

// MSL's __upper_map: the 256-entry toupper table of the current locale ("C": a-z map to
// A-Z, every other byte to itself). Game code indexes it directly with a byte.
extern const unsigned char msl_upper_map[256];
extern const unsigned char msl_lower_map[256];

// MSL wide-string entry points use 16-bit characters on the Mac.
uint32_t wcslen(const char16_t* text);
char16_t* wcscpy(char16_t* dst, const char16_t* src);
char16_t* wcsncpy(char16_t* dst, const char16_t* src, uint32_t count);
char16_t* wcscat(char16_t* dst, const char16_t* src);
char16_t* wcsncat(char16_t* dst, const char16_t* src, uint32_t count);
int32_t wcsncmp(const char16_t* a, const char16_t* b, uint32_t count);
const char16_t* wcsstr(const char16_t* text, const char16_t* needle);

// swprintf with 16-bit characters (the Mac's wchar_t): formats into at most `size` characters, the last of them the terminator
// (nothing at all is written for a size of 0), and answers the length written, or -1 when the text did not fit. Conversions:
// %ls (a wide string) and %s (a plain, narrow string, widened a byte at a time), %d %i %u %x %X %c (the
// argument is a number; %c a character), %f %F %e %E %g %G (a double) and %%, with the flags
// - + space # 0, a width and a precision (a number or *). Used by the game for its wide texts (platform/msl.cpp).
int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...);
