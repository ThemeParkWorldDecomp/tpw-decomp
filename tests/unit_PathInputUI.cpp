// Test library for the build panel's small UI callees (unit v5L06; libtpw_unit_PathInputUI.dylib, tests/test_PathInputUI.py), main app:
//   src/game/ui/ui_main.cpp   UIMain_TickRidePreview, UIMain_NotEnoughDough
//   src/game/ui/ui_ridepanel.cpp    RemoveRideListPanel
//   src/game/ui/InterfaceTimer.cpp, src/game/ui/InterfaceWindow.cpp      InterfaceWindow::FinishWithClientObjectSurface, InterfaceTimer::SetSlice,
//                                  InterfaceWindow::GetClientObjectSurface (unit v5L16)
//
// The callees (TickRidePreview, SOUND_SendEvent, UI_SendMessage, the draw object's Refresh) are fakes that append one event each:
// a code and its arguments, pointers as the 32-bit labels the test gave them.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <new>

#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "platform/arena.h"

// ---- globals of other files
sRidePreviewInfo* gRidePreview;
InterfaceWindow* gRideListPanel;
int32_t gSoundBanks[8];

namespace {
int64_t gEvents[64];
int32_t gEventCount;
int32_t gAnswer;
void Ev(std::initializer_list<int64_t> values) {
    for (int64_t v : values) {
        if (gEventCount < 64) gEvents[gEventCount++] = v;
    }
}
uint32_t L(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
template <typename T>
T* P(uint32_t label) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(label));
}

int32_t gSizes[2];
const int32_t* gArea;   // the case's words: pixel word, NULL bits, -, -, width, height

int64_t Kind(const void* p, const void* want) { return p == nullptr ? 0 : (p == want ? 1 : -1); }

class FakeDrawObject : public InterfaceDrawObject {
public:
    FakeDrawObject() : InterfaceDrawObject(nullptr) {}
    void Refresh() override { Ev({4, 77}); }
    int32_t GetWritableDataArea(int32_t* a, int32_t* b) override;
};
FakeDrawObject gObject;

int32_t FakeDrawObject::GetWritableDataArea(int32_t* a, int32_t* b) {
    Ev({7, this == &gObject ? 1 : -1, Kind(a, (gArea[1] & 1) != 0 ? nullptr : &gSizes[0]), Kind(b, (gArea[1] & 2) != 0 ? nullptr : &gSizes[1])});
    if (a != nullptr) *a = gArea[4];
    if (b != nullptr) *b = gArea[5];
    return gArea[0];
}
}  // namespace

// ---- the fakes
void TickRidePreview(sRidePreviewInfo* info) { Ev({1, L(info)}); }
int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    Ev({2, entity, bank, event, a, b, c});
    return gAnswer;
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    Ev({3, L(window), message, a, b});
    return gAnswer;
}
// The rectangles inside the fake draw object (src/game/ui/CoolRegion.cpp, src/game/ui/CoolRect.cpp is not in the unit either).
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
// InterfaceDrawObject's members the fake object's vtable names (src/game/ui/InterfaceDrawObject.cpp, src/game/ui/InterfaceDrawEngine.cpp is not in the unit).
InterfaceDrawObject::InterfaceDrawObject(void* data) : mData(data) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() { Ev({5}); }
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

extern "C" {

// Runs function `fn` (0 TickRidePreview, 1 NotEnoughDough, 2 RemoveRideListPanel, 3 FinishWithClientObjectSurface, 4 SetSlice,
// 5 GetClientObjectSurface) with
// the globals set from `w` (preview label, ui bank, panel label, has a client draw object, the slice argument, the timer's old
// slice); writes the events to `out` (at most 64) and answers their count. SetSlice's timer slice comes back in *slice.
int32_t tpw_piu_run(int32_t fn, const int32_t* w, int64_t* out, int32_t* slice) {
    gEventCount = 0;
    gAnswer = 12345;
    gRidePreview = P<sRidePreviewInfo>(static_cast<uint32_t>(w[0]));
    for (int32_t i = 0; i < 8; ++i) gSoundBanks[i] = -1;
    gSoundBanks[3] = w[1];
    gRideListPanel = P<InterfaceWindow>(static_cast<uint32_t>(w[2]));
    switch (fn) {
    case 0: UIMain_TickRidePreview(); break;
    case 1: UIMain_NotEnoughDough(); break;
    case 2: RemoveRideListPanel(); break;
    case 3: {
        alignas(InterfaceWindow) static unsigned char storage[sizeof(InterfaceWindow)];
        auto* window = reinterpret_cast<InterfaceWindow*>(storage);
        window->mClientDrawObject = w[3] != 0 ? &gObject : nullptr;
        window->FinishWithClientObjectSurface();
        break;
    }
    case 5: {
        alignas(InterfaceWindow) static unsigned char storage[sizeof(InterfaceWindow)];
        auto* window = reinterpret_cast<InterfaceWindow*>(storage);
        window->mClientDrawObject = w[3] != 0 ? &gObject : nullptr;
        gArea = w;
        gSizes[0] = gSizes[1] = 0x5a5a5a5a;
        uint8_t* pixels = window->GetClientObjectSurface((w[1] & 1) != 0 ? nullptr : &gSizes[0], (w[1] & 2) != 0 ? nullptr : &gSizes[1]);
        // The pixels as the Mac's word (the low half) and where they are: 1 in the 4 GB from the arena's base up (unit v5L28: the
        // engine blocks are arena blocks), 0 for none, -1 elsewhere (the word widened on its own).
        const uintptr_t at = reinterpret_cast<uintptr_t>(pixels);
        const int64_t where = pixels == nullptr ? 0 : (at - reinterpret_cast<uintptr_t>(tpw::ArenaBase()) <= 0xFFFFFFFFu ? 1 : -1);
        Ev({8, static_cast<int64_t>(static_cast<uint32_t>(at)), gSizes[0], gSizes[1], where});
        break;
    }
    case 4: {
        alignas(InterfaceTimer) static unsigned char storage[sizeof(InterfaceTimer)];
        auto* timer = reinterpret_cast<InterfaceTimer*>(storage);
        timer->mSlice = w[5];
        timer->mId = 0x5a5a;
        timer->SetSlice(w[4]);
        *slice = timer->mSlice;
        Ev({6, timer->mId});
        break;
    }
    }
    for (int32_t i = 0; i < gEventCount; ++i) out[i] = gEvents[i];
    return gEventCount;
}

}  // extern "C"
