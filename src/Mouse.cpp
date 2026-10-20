// Mouse.cpp (main): AssignMouseMode 0x1007e2e0 (312 B), CMMDefault constructor 0x1007ea78 (36 B),
// destructor 0x100813e8 (88 B), clicks 0x1007eac4 / 0x1007eaf8 (4 B each), GetMode 0x10081468 (8 B), OnAssign 0x1007eb2c (60 B).
#include <new>
#include "Mouse.h"
#include "FS_Online.h"
#include "UIInterface.h"
#include "Debug.h"

CMouse* gMouse;                 // 0x102dc1f4
CMouseMode* gMouseMode;         // 0x102dc1f0
int32_t gMouseWord1E0;          // 0x102dc1e0
int32_t gMouseWord1E4;          // 0x102dc1e4

void CMouse::AssignMouseMode(CMouseMode* mode) {
    if (gMouseMode != nullptr) {
        gMouseMode->OnRelease();
        // OnRelease can replace or clear the global.
        if (gMouseMode != nullptr) delete gMouseMode;
    }
    if (TheGameType().mType == kGameTypeOnline && mode != nullptr) {
        if (mode->GetMode() == 1) {
            delete mode;
            mode = new (std::nothrow) CMMOnline;
        }
    }
    gMouseMode = mode;
    if (mode != nullptr) mode->OnAssign();
}

CMMDefault::CMMDefault() { mWord4 = 1; }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {
    gUIInterface.CheckMouseContext(1);
    CTRACE(3, "Assigning CMMDefault\n");   // 0x1023df05
}
