// Test entry point for src/game/TPGlobals.cpp (library unit_TPGlobals, with the real bullfrog
// strings): every string of a freshly constructed TPGlobals, in memory order.
#include <cstdio>
#include <cstring>
#include <new>

#include "TPGlobals.h"

// String members the unit's sources reference and nothing here reaches.
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

extern "C" {

// Writes one line per string, "c text" or "w text" (16-bit characters as their low
// bytes), walking the 159 string members from the first to the last by address, and
// returns how many there are. The object is built in place for the call.
int32_t tpw_tpglobals_strings(char* out, uint32_t size) {
    static TbDynamicStringTemplate<char> narrow;
    static TbDynamicStringTemplate<char16_t> wide;
    alignas(TPGlobals) static unsigned char storage[sizeof(TPGlobals)];
    auto* globals = new (storage) TPGlobals;
    const size_t step = sizeof(TbDynamicStringTemplate<char>);
    auto* first = reinterpret_cast<unsigned char*>(&globals->mF4024);
    auto* last = reinterpret_cast<unsigned char*>(&globals->mF4C7C);
    int32_t count = 0;
    size_t used = 0;
    out[0] = '\0';
    for (auto* slot = first; slot <= last; slot += step, ++count) {
        char line[160];
        if (std::memcmp(slot, &narrow, sizeof(void*)) == 0) {
            std::snprintf(line, sizeof line, "c %s\n", reinterpret_cast<TbDynamicStringTemplate<char>*>(slot)->mData);
        } else if (std::memcmp(slot, &wide, sizeof(void*)) == 0) {
            auto* s = reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(slot);
            char text[140];
            int32_t n = 0;
            for (; n < s->mLength && n < 139; ++n) text[n] = static_cast<char>(s->mData[n]);
            text[n] = '\0';
            std::snprintf(line, sizeof line, "w %s\n", text);
        } else {
            std::snprintf(line, sizeof line, "? not a string\n");
        }
        if (used + std::strlen(line) + 1 < size) {
            std::strcpy(out + used, line);
            used += std::strlen(line);
        }
    }
    globals->~TPGlobals();
    return count;
}

}  // extern "C"
