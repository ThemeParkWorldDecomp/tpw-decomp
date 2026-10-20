#pragma once
#include <cstdint>
struct GrafPort;
namespace NS_MacDoze {
int32_t InterlockedIncrement(int32_t*);
int32_t InterlockedDecrement(int32_t*);
// macdoze 0x10006ef8 (import MessageBoxW__10NS_MacDozeFP8GrafPortPCwPCwUi): a message box with a
// UTF-16 text and title; `flags` are the Windows MB_ flags (the game passes 0x10010: MB_ICONERROR |
// MB_SETFOREGROUND). Answers the button pressed (IDOK, 1, always: the host shows no box and writes
// the title and text to the platform log; platform/dialogs.cpp).
int32_t MessageBoxW(GrafPort* owner, const char16_t* text, const char16_t* title, uint32_t flags);
}
namespace SamsUtilities {
// Inlined endian conversions in ltms. Identity on little endian hosts.
template<class T> void USwapInplace16(T& value) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    value = static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(value)));
#endif
}
template<class T> void USwapInplace32(T& value) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    value = static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(value)));
#endif
}
}
