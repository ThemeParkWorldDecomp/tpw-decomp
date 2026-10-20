// CFSLoadSave::Load and LoadInternal (FS_LoadSave.cpp, main app 0x101fc408 and 0x101fc5e0, SimTheme Park 2.04b3; unit w117). A file of its own so
// that the test of LoadMostRecent (src/FS_LoadSave.cpp) does not have to link them.
#include "FS_LoadSave.h"

#include "FS_UserInfo.h"
#include "SaveLoad.h"
#include "TPGlobals.h"
#include "TPString.h"

// Ghidra says void; the listing returns what LoadInternal answered.
int32_t CFSLoadSave::Load(const TbDynamicStringTemplate<char16_t>& name) { return LoadInternal(name, gTPGlobals.mF4420); }

// Loads "<the logged-in player's folder for the current theme>:<name><extension>" as a park (no header, type kLoadPark); the extension
// Load gives is ".TPWS". The user info is made on first use (the same idiom as LoadMostRecent). 1 on success, 0 on failure.
int32_t CFSLoadSave::LoadInternal(const TbDynamicStringTemplate<char16_t>& name, const TbDynamicStringTemplate<char16_t>& extension) {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    // (the original assigns the returned string into a string it made first and destroys the temporary; unrecorded either way)
    TbDynamicStringTemplate<char16_t> path(gUserInfo->ReturnUsersPathWithTheme());
    path += gTPGlobals.mWideSeparator;
    path += name;
    path += extension;
    TbDynamicStringTemplate<char> narrow;
    UniStringToString(narrow, path);
    return gSaveLoad.Load(narrow.mData, nullptr, CSaveLoad::kLoadPark);
}
