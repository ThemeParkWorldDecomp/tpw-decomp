// LbString: PARTIAL bullfrog library string helpers (WinScrn.cpp range).
#pragma once
#include <cstdint>

// Copies src into dst (size bytes with the NUL). Returns 1 if the whole string fitted;
// otherwise copies size - 1 characters, terminates and returns 0 (also when the string
// is exactly size - 1 long). 0x10066200.
int32_t LbString_strnzcpy(char* dst, const char* src, uint32_t size);
