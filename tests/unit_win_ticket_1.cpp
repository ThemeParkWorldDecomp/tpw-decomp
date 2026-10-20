// Test library for the keyboard actions win_ticket_0..6 and enter_area_delete_mode (unit v5030), main app:
//   src/ui_Postcard_panel_win_ticket_1.cpp
//
// Callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "keytable_actions.h"
#include "FS_MasterSave.h"
#include "Mouse.h"

using namespace calllog;

CFSGlobalMasterSave* gMasterSave;
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;

// The real constructor builds string members whose code this library does not link: the fake takes its symbol directly.
extern "C" void tpw_wt_ms_ctor(void* self) asm("__ZN19CFSGlobalMasterSaveC1Ev");
extern "C" void tpw_wt_ms_ctor(void* self) { ev("MSCtor", {newlab(self)}); }
int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeLocal type) {
    ev("EarnedLocal", {lab(this), static_cast<int32_t>(type)});
    return 0;
}
int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeGlobal type, int32_t n) {
    ev("EarnedGlobal", {lab(this), static_cast<int32_t>(type), n});
    return 0;
}
extern "C" void tpw_wt_pd_ctor(void* self, int32_t mode) asm("__ZN11CMMPathDrawC1E10CELL_MODES");
extern "C" void tpw_wt_pd_ctor(void* self, int32_t mode) { ev("PathDrawCtor", {newlab(self), mode}); }
extern "C" void tpw_wt_df_ctor(void* self) asm("__ZN10CMMDefaultC1Ev");
extern "C" void tpw_wt_df_ctor(void* self) { ev("DefaultCtor", {newlab(self)}); }
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("Assign", {lab(this), lab(mode)}); }

extern "C" {

uint64_t tpw_wt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMasterSave", &gMasterSave},
        {"gMouse", &gMouse},
        {"w1E0", &gMouseWord1E0},
        {"w1E4", &gMouseWord1E4},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

int32_t tpw_wt_call(int32_t which) {
    switch (which) {
        case 0: return win_ticket_0();
        case 1: return win_ticket_1();
        case 2: return win_ticket_2();
        case 3: return win_ticket_3();
        case 4: return win_ticket_4();
        case 5: return win_ticket_5();
        case 6: return win_ticket_6();
        default: return enter_area_delete_mode();
    }
}

}  // extern "C"
