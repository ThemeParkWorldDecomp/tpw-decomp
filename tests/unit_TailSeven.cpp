// Test library for two leaves of the main app (unit v5L16; libtpw_unit_TailSeven.dylib, tests/test_TailSeven.py):
//   src/TailSeven.cpp   UIMain_GetObjectName, CalcZTrans
//
// CObject::GetUIName is a fake that records the object (its 32-bit label), whether the buffer is gUIMainConvertBuffer and the size,
// then writes the case's four characters. gUIMainConvertBuffer (src/ui_main.cpp) is defined here.
#include <cstdint>
#include <initializer_list>

#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "ui_main.h"

char16_t gUIMainConvertBuffer[0x4000];

namespace {
int64_t gEvents[16];
int32_t gEventCount;
const uint16_t* gChars;
void Ev(std::initializer_list<int64_t> values) {
    for (int64_t v : values) {
        if (gEventCount < 16) gEvents[gEventCount++] = v;
    }
}
}  // namespace

void CObject::GetUIName(char16_t* out, uint32_t size) {
    Ev({1, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this))), out == gUIMainConvertBuffer ? 1 : -1, size});
    for (int32_t i = 0; i < 4; ++i) out[i] = static_cast<char16_t>(gChars[i]);
}

extern "C" {

// io: the four characters GetUIName writes and the guard of the buffer's fifth; comes back as the buffer's first five. out: the
// events, then 1 when the answer is the buffer. Answers the count of out's words.
int32_t tpw_t7_name(uint32_t object, uint16_t* io, int64_t* out) {
    gEventCount = 0;
    gChars = io;
    for (int32_t i = 0; i < 4; ++i) gUIMainConvertBuffer[i] = 0x5a5a;
    gUIMainConvertBuffer[4] = static_cast<char16_t>(io[4]);
    char16_t* answer = UIMain_GetObjectName(reinterpret_cast<CObject*>(static_cast<uintptr_t>(object)));
    for (int32_t i = 0; i < 5; ++i) io[i] = static_cast<uint16_t>(gUIMainConvertBuffer[i]);
    for (int32_t i = 0; i < gEventCount; ++i) out[i] = gEvents[i];
    out[gEventCount] = answer == gUIMainConvertBuffer ? 1 : 0;
    return gEventCount + 1;
}

float tpw_t7_ztrans(int32_t z) { return CalcZTrans(z); }

}  // extern "C"
