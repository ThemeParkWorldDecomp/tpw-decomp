// Unit v51117: FS_MasterSave.cpp's static initialiser, the online callbacks that make the online system on first use, and keyboard actions.
#include <new>

#include "Debug.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "TPMain.h"
#include "ui_ime.h"
#include "lobby.h"
#include "LLCellSystem.h"
#include "keytable_actions.h"
#include "TbDynamicStringTemplate.h"

// 0x1016f35c
void __sinit_FS_MasterSave_cpp() {
    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyWString);
    auto* s = new (gMasterSaveString) TbDynamicStringTemplate<char16_t>(
        reinterpret_cast<const char16_t*>(reinterpret_cast<const char*>(gMasterSaveWideText) + 0x5f));
    __register_global_object(s, destroy, gMasterSaveNode);
}

// 0x10171dfc (also defined weakly in src/FS_Online_init_CFSOnlineSystem_LoginConnectionCallback.cpp: same body)
__attribute__((weak)) void FSOnlineNewsConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->NewsConnectionCallback(a, b, c, d);
}

// 0x10174b6c
void SatelliteOnlineParkInfoPanelCancelCallback() { parkinfo_callee::DownloadGlobeSlot4c(gLobbyScreens->mDownloadGlobe); }

// 0x10175748
void FSOnlineUploadServerConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->UploadServerConnectionCallback(a, b, c, d);
}

// 0x10175d3c
void RequestFTPPostLoginCallbackFn(int32_t error, void* context) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->RequestFTPPostLoginCallback(error, context);
}

// 0x10175fac
void GotoPlayerByIDFTPPostLoginCallbackFn(int32_t error, void* context) {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
    gOnlineSystem->GotoPlayerByIDFTPPostLoginCallback(error, context);
}

int32_t dbgchan9() { return 1; }
int32_t dbgchan8() { return 1; }
int32_t dbgchan7() { return 1; }
int32_t dbgchan6() { return 1; }
int32_t dbgchan5() { return 1; }
int32_t dbgchan4() { return 1; }
int32_t dbgchan3() { return 1; }
int32_t dbgchan2() { return 1; }
int32_t dbgchan1() { return 1; }
int32_t dbgchan0() { return 1; }
int32_t debug_track_cell_dirs() { return 1; }
int32_t debug_hoarding_cell_neighbs() { return 1; }
int32_t debug_track_cell_neighbs() { return 1; }
int32_t debug_cell_dirs() { return 1; }
int32_t debug_cell_neighbs() { return 1; }
int32_t force_ride_fix() { return 1; }
int32_t force_ride_condemned() { return 1; }
int32_t force_ride_worn() { return 1; }
int32_t toggle_cell_flash() { return 1; }
int32_t next_debug_text_mode() { return 1; }
int32_t set_allow_loops_no_research() { return 1; }
int32_t select_debug_person() { return 1; }

// 0x1014f43c
int32_t save_complete_hoardings() {
    LLCC_WriteCompleteHoardingsToBalanceFile();
    return 1;
}

// 0x1014f3ec
int32_t save_hoardings() {
    LLCC_WriteHoardingsToBalanceFile();
    return 1;
}
