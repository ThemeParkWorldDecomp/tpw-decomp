// Keyboard actions: golden ticket cheats and area delete mode (main 0x1014ec48 .. 0x1014f148).
#include <cstdint>

#include "keytable_actions.h"
#include "FS_MasterSave.h"
#include "Mouse.h"

namespace {

CFSGlobalMasterSave* MasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave();
    return gMasterSave;
}

int32_t WinLocal(int32_t type) {
    MasterSave()->EarnedTicket(static_cast<GoldenTicketTypeLocal>(type));
    return 1;
}

}  // namespace

int32_t win_ticket_0() { return WinLocal(0); }   // 0x1014ed40
int32_t win_ticket_1() { return WinLocal(1); }   // 0x1014edd8
int32_t win_ticket_2() { return WinLocal(2); }   // 0x1014ee70
int32_t win_ticket_3() { return WinLocal(3); }   // 0x1014ef08
int32_t win_ticket_4() { return WinLocal(4); }   // 0x1014efa0
int32_t win_ticket_5() { return WinLocal(5); }   // 0x1014f038

int32_t win_ticket_6() {   // 0x1014f0d0
    MasterSave()->EarnedTicket(static_cast<GoldenTicketTypeGlobal>(0), 100);
    return 1;
}

int32_t enter_area_delete_mode() {   // 0x1014ec48
    CMMPathDraw* mode = new CMMPathDraw(static_cast<CELL_MODES>(0x3a));
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse();
        CMMDefault* def = new CMMDefault();
        mouse->AssignMouseMode(def);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(mode);
    return 1;
}
