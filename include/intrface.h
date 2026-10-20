// intrface.cpp (main app): CIntrface, the park's pointer and overlay state. PARTIAL;
// untranslated.
#pragma once
#include <cstddef>
#include <cstdint>

class CThing;

class CIntrface {
public:
    void DrawFPSCounter(float fps);         // 0x1007dae8
    void Process();                         // 0x1007db78, src/intrface_process.cpp (unit w131): mProcessCount += 1, then mPointedThing = 0
    void DoPointedThingComputations();      // 0x1007dbb8, src/intrface_pointed.cpp (unit w133)
    void SetCurrentThing(CThing* thing);    // 0x1007dde8 (8 bytes, src/intrface_current.cpp, unit v5L06): mPointedThing = thing

    // On the Mac three 32-bit pointers; native pointers here, so the object is larger than the Mac's (the layout assert below is the Mac's
    // offsets of the words other code reads and writes).
    CThing* mPointedThing;          // 0x00  Process clears it; DoPointedThingComputations ends by giving it the thing under the pointer
    CThing* mPointedStaffOrGuest;   // 0x04  the pointed thing when it is a staff member, a guest or such (kinds 1, 4 to 8, 0x12)
    CThing* mThingUnderPointer;     // 0x08  the thing in the cell under the pointer
    int32_t mProcessCount;  // 0x0c  Process adds 1 (names guessed)
    uint8_t mF10[0x14];
    // Unit w139: GetMapCellFromPointerWorldPosn writes mF24 (the map cell id under the pointer, 0 when outside the level).
    uint16_t mF24;          // 0x24  the id GAMESYSTEM_Tick counts neighbours of for the chatter level
    uint8_t mF26[2];
    int32_t mShowFPS;       // 0x28
};
// The Mac's offsets hold where the pointers are 32-bit; with native pointers the words after them move (nothing else reads them by offset).
static_assert(offsetof(CIntrface, mThingUnderPointer) == 2 * sizeof(void*), "three pointers first");
static_assert(sizeof(void*) != 4 || (offsetof(CIntrface, mF24) == 0x24 && offsetof(CIntrface, mProcessCount) == 0xc &&
                                      offsetof(CIntrface, mThingUnderPointer) == 8), "the Mac's layout");
extern CIntrface gIntrface;   // 0x102dc1ac, original name unknown
// Unit v51063 (src/LLCR_OverRide_LLCR_OverRide_3_LLCR_GetRideCell.cpp): the static initialiser of intrface.cpp.
extern int32_t gIntrfaceDC1D8;   // 0x102dc1d8  0x10000 after __sinit_intrface_cpp
extern int32_t gIntrfaceDC1DC;   // 0x102dc1dc  0 after __sinit_intrface_cpp
void __sinit_intrface_cpp();     // 0x1007de28 (52 bytes): also mF24 = 1, mShowFPS = 0
