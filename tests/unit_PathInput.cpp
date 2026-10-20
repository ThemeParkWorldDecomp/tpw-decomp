// Test library for the path tool's pointer and selection accessors (unit v5L06; libtpw_unit_PathInput.dylib, tests/test_PathInput.py),
// main app:
//   src/UI_Base_mouse.cpp           UI_GetMouseXY
//   src/Debug_pointer.cpp           SetPotentiallyHighlightedRide
//   src/TPRide_pointed.cpp          GetPointedAtRide, LockPointedAtRide
//   src/intrface_current.cpp        CIntrface::SetCurrentThing
//   src/ui_allKidsPanel_select.cpp  UI_GetSelectedPeep, UI_GetSelectedObject, UI_SetSelectedObject, UI_CheckSelected,
//                                   UI_SetSelectedPeep (unit v5L16)
//
// No callees. The globals the functions read and write are defined here; the test loads them from eleven words (pointers as
// 32-bit labels, never dereferenced) and reads them back the same way.
#include <cstdint>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "intrface.h"
#include "ui_main.h"

// ---- globals of other files
int16_t gUIPointerX;
int16_t gUIPointerY;
tag_sRideAnimSet* gHighlightedRide;
tag_sRideAnimSet* gPotentialHighlightedRide;
int32_t gRideHighlightKeep;
CIntrface gIntrface;
void* gUISelectedThing;
void* gUISelectedPeep;
void* gUISelectedObject;

namespace {
template <typename T>
T* P(uint32_t label) {
    return reinterpret_cast<T*>(static_cast<uintptr_t>(label));
}
uint32_t L(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

extern "C" {

// w: pointer x, pointer y (as 16-bit), highlighted ride, potential ride, keep word, the interface's three pointer words, the
// selection's thing, peep and object.
void tpw_pi_load(const uint32_t* w) {
    gUIPointerX = static_cast<int16_t>(w[0]);
    gUIPointerY = static_cast<int16_t>(w[1]);
    gHighlightedRide = P<tag_sRideAnimSet>(w[2]);
    gPotentialHighlightedRide = P<tag_sRideAnimSet>(w[3]);
    gRideHighlightKeep = static_cast<int32_t>(w[4]);
    gIntrface.mPointedThing = P<CThing>(w[5]);
    gIntrface.mPointedStaffOrGuest = P<CThing>(w[6]);
    gIntrface.mThingUnderPointer = P<CThing>(w[7]);
    gUISelectedThing = P<void>(w[8]);
    gUISelectedPeep = P<void>(w[9]);
    gUISelectedObject = P<void>(w[10]);
}

void tpw_pi_store(uint32_t* w) {
    w[0] = static_cast<uint16_t>(gUIPointerX);
    w[1] = static_cast<uint16_t>(gUIPointerY);
    w[2] = L(gHighlightedRide);
    w[3] = L(gPotentialHighlightedRide);
    w[4] = static_cast<uint32_t>(gRideHighlightKeep);
    w[5] = L(gIntrface.mPointedThing);
    w[6] = L(gIntrface.mPointedStaffOrGuest);
    w[7] = L(gIntrface.mThingUnderPointer);
    w[8] = L(gUISelectedThing);
    w[9] = L(gUISelectedPeep);
    w[10] = L(gUISelectedObject);
}

// Calls function `fn` (the test's numbering) with `a` (`b` is unused); returns its answer (0 for a void function).
// UI_GetMouseXY writes xy[0] and xy[1].
uint32_t tpw_pi_call(int32_t fn, uint32_t a, uint32_t b, int16_t* xy) {
    switch (fn) {
    case 0: UI_GetMouseXY(xy[0], xy[1]); return 0;
    case 1: SetPotentiallyHighlightedRide(P<tag_sRideAnimSet>(a)); return 0;
    case 2: return L(GetPointedAtRide());
    case 3: LockPointedAtRide(static_cast<int32_t>(a)); return 0;
    case 4: gIntrface.SetCurrentThing(P<CThing>(a)); return 0;
    case 5: return L(UI_GetSelectedPeep());
    case 6: return L(UI_GetSelectedObject());
    case 7: UI_SetSelectedObject(P<void>(a)); return 0;
    case 8: UI_CheckSelected(); return 0;
    case 9: UI_SetSelectedPeep(P<CPeep>(a)); return 0;
    }
    return 0xdeadbeef;
}

}  // extern "C"
