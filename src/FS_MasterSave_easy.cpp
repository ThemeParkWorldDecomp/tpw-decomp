// FS_MasterSave.cpp (main app), unit w48: CFSGlobalMasterSave::SetEasyModeUser 0x1016a540 (8 bytes), the byte IsEasyModeUser
// (src/FS_login.cpp) reads. In its own file so the user-slot unit's test library can link it without the login code.
#include "FS_MasterSave.h"

// The Mac stores the LOW BYTE of the register (stb): 2 stays 2, 256 becomes 0. The parameter was a bool until the triage of
// 2026-10-12 (test_FSUserSlots easy_edge); every translated caller passes a comparison's 0 or 1.
void CFSGlobalMasterSave::SetEasyModeUser(int32_t easyMode) { mEasyMode = static_cast<uint8_t>(easyMode); }
