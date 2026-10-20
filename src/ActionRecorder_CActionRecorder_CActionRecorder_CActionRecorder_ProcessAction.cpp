// CActionRecorder::ProcessAction overloads (unit v5254): replay one recorded action by calling the game function it stands for.
#include <cstdint>

#include "analyser.h"
#include "ActionRecorder.h"
#include "Debug.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"

// Untranslated callees (stand-ins in the launcher).
void SetPlaceNormalPylon();                                   // 0x10058b68
void SetPlaceSpecialPylon();                                  // 0x10058bb8
void LayoutRegenerateTrack();                                 // 0x10058e48
void LayoutDeleteLastTrackBrace();                            // 0x10058f4c
void LayoutDeleteMultiPylons(uint16_t a, uint16_t b);         // 0x10058cf8
void LayoutReplayCoasterSpeed(uint32_t a, uint32_t b);        // 0x10059120
void LayoutSetMovePylon(uint16_t a, uint16_t b, uint16_t c);  // 0x10058da4

#define AR_ERR "Erroneous stuff going down\n"

// 0x10145b28
void CActionRecorder::ProcessAction(int32_t action) {
    CTRACE(2, "AR replay - %d\n", action);
    switch (action) {
        case 0x37: SetPlaceNormalPylon(); break;
        case 0x38: SetPlaceSpecialPylon(); break;
        case 0x40: LayoutRegenerateTrack(); break;
        case 0x41: LayoutDeleteLastTrackBrace(); break;
        default: CTRACE(2, AR_ERR); break;
    }
}

// 0x10145d34
void CActionRecorder::ProcessAction(int32_t action, uint8_t a, uint8_t b, uint16_t c) {
    CTRACE(2, "AR replay - %d (%d, %d, %d)\n", action, a, b, c);
    if (action == 0x2d) LLCM_LeftMouseButtonUp(a, b, c, 0);
    else if (action == 0x28) LLCM_LeftMouseButtonDown(a, b, c, 0);
    else CTRACE(2, AR_ERR);
}

// 0x10145c24
void CActionRecorder::ProcessAction(int32_t action, uint8_t a, uint8_t b) {
    CTRACE(2, "AR replay - %d (%d, %d)\n", action, a, b);
    if (action == 0x14) LLCM_LeftMouseButtonUp(a, b, 0, 0);
    else if (action == 0xf) LLCM_LeftMouseButtonDown(a, b, 0, 0);
    else CTRACE(2, AR_ERR);
}

// 0x10146058
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint8_t b) {
    CTRACE(2, "AR replay - %d (%d, %d)\n", action, a, b);
    if (action == 5) {
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
        id.mIndex = a;
        LLCC_SystemSetRide(id, b);
    } else {
        CTRACE(2, AR_ERR);
    }
}

// 0x10146140
void CActionRecorder::ProcessAction(int32_t action, float a, uint16_t b) {
    CTRACE(2, "AR replay - %d (%g, %d)\n", action, static_cast<double>(a), b);
    if (action == 0x3a) LayoutSetRotatePylon(a, b);
    else if (action == 0x39) LayoutSetLoftPylon(a, b);
    else CTRACE(2, AR_ERR);
}

// 0x10146248
void CActionRecorder::ProcessAction(int32_t action, float a, float b, uint16_t c) {
    CTRACE(2, "AR replay - %d (%g, %g, %d)\n", action, static_cast<double>(a), static_cast<double>(b), c);
    if (action == 0x3b) LayoutSetWobblePylon(a, b, c);
    else CTRACE(2, AR_ERR);
}

// 0x1014634c
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint16_t b) {
    CTRACE(2, "AR replay - %d (%d, %d)\n", action, a, b);
    if (action == 0x45) LayoutReplayCoasterSpeed(a, b);
    else if (action == 0x3f) LayoutDeleteMultiPylons(a, b);
    else CTRACE(2, AR_ERR);
}

// 0x1014644c
void CActionRecorder::ProcessAction(int32_t action, uint16_t a, uint16_t b, uint16_t c) {
    CTRACE(2, "AR replay - %d (%d, %d, %d)\n", action, a, b, c);
    if (action == 0x3c) LayoutSetMovePylon(a, b, c);
    else CTRACE(2, AR_ERR);
}
