// CFSGlobalMasterSave constructor 0x101694dc (156 bytes) and ResetGlobalData 0x10169a88 (96); existing bodies, main, unit w43.
#include "FS_MasterSave.h"

CFSGlobalMasterSave::CFSGlobalMasterSave() { ResetGlobalData(); }

void CFSGlobalMasterSave::ResetGlobalData() {
    for (int32_t i = 0; i < 4; ++i) mGlobalTickets[i] = 0;
    for (int32_t i = 0; i < 2; ++i) mSecretTickets[i] = 0;
    mTicketsSpent = 0;
    mExtraKeys = 0;
    mEasyMode = 0;
    mSwearFilter = 1;
    mFirstTimePlayer = 1;
}
