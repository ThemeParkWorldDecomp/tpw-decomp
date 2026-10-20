// Unit v51048: eight small functions of the main app (Game.cpp, ActionRecorder.cpp, ui_visitorStatus.cpp's list instance, Peep.cpp's park time,
// UI_Base.cpp's bar and deferred message).
#include <cstdint>
#include <cstring>
#include <new>

#include "ActionRecorder.h"
#include "Advisor.h"
#include "Clock.h"
#include "Game.h"
#include "ParkTime.h"
#include "TPSound.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "WorldThings.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// 0x1014a478 (136 bytes): the theme's folder name as a narrow string: converted to a wide one, which the wide overload turns into the player's
// language name when the conversion answered nonzero.
void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    TbDynamicStringTemplate<char16_t> converted;
    if (StringToUniString(converted, theme) != 0) GetLocalisedThemeName(converted, out);
}

// 0x10149a74 (212 bytes): only while mF3C is 1. The UI and the clock toggle; then a paused game resumes (the sounds as the last pause left
// them: all of them when mF20 is set, else the advisor and the 3D sounds) or a running one pauses (mF20 takes the argument).
void CGame::TogglePause(bool allSounds) {
    if (mF3C != 1) return;
    gUIInterface.TogglePause();
    gClock.TogglePause();
    if (mF1C == 1) {
        mF1C = 0;
        if (mF20 != 0) {
            SOUND_UnPause();
        } else {
            ADV_UnPause();
            SOUND_SetPauseOn3DSoundsOnly(0);
        }
    } else {
        mF1C = 1;
        mF20 = allSounds;
        if (mF20 != 0) {
            SOUND_Pause();
        } else {
            ADV_Pause();
            SOUND_SetPauseOn3DSoundsOnly(1);
        }
    }
}

// Raw storage (the Mac's string is 0x14 bytes, the recorder's records unknown); names invented.
alignas(16) uint8_t gActionRecorderString[64];        // 0x1037f024
alignas(16) uint8_t gActionRecorderStringNode[32];    // 0x1037f038
alignas(16) uint8_t gActionRecorderNode[32];          // 0x1037f018
alignas(16) uint8_t gActionRecorderDtor[16];          // stands for the destructor word 0x10266e58

static const char16_t kActionRecorderText[] = u"gms.dat";   // the text at 0x10248615

// 0x10146e9c (88 bytes): the file's static initialiser: a wide string built from the text and registered for exit with the string destructor,
// then the action recorder constructed and registered with its own destructor.
void __sinit_ActionRecorder_cpp() {
    void* str = ::new (gActionRecorderString) TbDynamicStringTemplate<char16_t>(kActionRecorderText);
    __register_global_object(str, reinterpret_cast<void*>(&UIMain_DestroyWString), gActionRecorderStringNode);
    gActionRecorder.Construct();
    __register_global_object(&gActionRecorder, gActionRecorderDtor, gActionRecorderNode);
}

// 0x10141f1c (100 bytes): the pending-delete list. The nodes are packed on the Mac (id at +0, next at +2, previous at +6); the host's
// are CNodes with native pointers. Head and tail are cleared first, then every node is freed (its next read before the free).
template<> void
TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>::RemoveAll() {
    CNode* node = mHead;
    mTail = nullptr;
    mHead = nullptr;
    while (node != nullptr) {
        CNode* next = node->mNext;
        ::operator delete(node);
        node = next;
    }
    mCount = 0;
}

// 0x10112928 (132 bytes): turns times the funny seconds per real second as an unsigned 64-bit product, divided by 4 (signed 64-bit
// division, as the runtime's routine does), times 10,000,000 ticks.
TbTimeDiff CParkTime::GetFunnyTimeForGameTurns(uint32_t turns) const {
    const uint64_t product = static_cast<uint64_t>(turns) * mFunnySecsPerRealSec;
    const int64_t quarter = static_cast<int64_t>(product) / 4;
    TbTimeDiff span;
    span.mTicks = quarter * 10000000;
    return span;
}

// 0x101c887c (188 bytes): a changed value is clamped to 0 .. 0x400 and told to the draw object and the client draw object (Change, flags 8, a
// block whose first word is the value). 1 when the value changed, else 0.
int32_t InterfaceBar::SetValue(int32_t value) {
    if (value == mValue) return 0;
    mValue = value;
    if (mValue > 0x400) {
        mValue = 0x400;
    } else if (mValue < 0) {
        mValue = 0;
    }
    DOChange change;
    std::memcpy(change.mF00, &mValue, sizeof(int32_t));
    if (mDrawObject != nullptr) mDrawObject->Change(8, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(8, &change);
    return 1;
}

// 0x101c89a0 (128 bytes): the difference of the two values when both windows say they are the same kind (this one is asked first), else 0.
// The other window is read as a bar whatever it is.
int32_t InterfaceBar::operator-(InterfaceWindow& other) {
    const int32_t mine = WhatAmI();
    const int32_t theirs = other.WhatAmI();
    if (theirs != mine) return 0;
    return mValue - static_cast<InterfaceBar&>(other).mValue;
}

// 0x101c95f8 (120 bytes): messages 1 to 3 (unsigned compare of message - 1) are refused.
int32_t UI_PutDeferredMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b, uint32_t time) {
    if (gUITimedMessages == nullptr || window == nullptr || static_cast<uint32_t>(message - 1) <= 2) return 0;
    gUITimedMessages->Add(window, message, a, b, time);
    return 1;
}
