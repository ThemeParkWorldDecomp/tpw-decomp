// Unit v5L16 (the tail): two leaves of the main app (SimTheme Park 2.04b3). UIMain_GetObjectName (0x101810fc, 60 bytes, Mac file
// ui_main.cpp) and CalcZTrans (0x101879d0, 52 bytes, the draw objects' depth, Mac file ui_DrawObject.cpp).
#include <cstdint>

#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "ui_main.h"

// The object's interface name in the shared conversion buffer (at most 0x3fff characters); the answer is the buffer.
char16_t* UIMain_GetObjectName(CObject* object) {
    object->GetUIName(gUIMainConvertBuffer, 0x3fff);
    return gUIMainConvertBuffer;
}

// The depth of interface layer z: 1 - z / 40 in single precision (the Mac's int to double conversion is rounded to single by its
// fsubs, then fdivs and fsubs; the 40.0f is the data word at 0x102a7af8 that the engine mesh also reads).
float CalcZTrans(int32_t z) {
    constexpr float kDepthRange = 40.0f;
    constexpr float kOne = 1.0f;
    return kOne - static_cast<float>(z) / kDepthRange;
}
