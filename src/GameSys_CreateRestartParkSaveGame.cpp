#include "platform/msl.h"
#include "GameSys.h"
#include "FS_LoadSave.h"
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "TbError.h"
#include "bfl_FileSystem.h"

namespace {

// The original copies the temporary the users path call answers into a local, then destroys the temporary.
TbDynamicStringTemplate<char16_t> CopyUsersPathWithTheme() {
    TbDynamicStringTemplate<char16_t> temp = gUserInfo->ReturnUsersPathWithTheme();
    return TbDynamicStringTemplate<char16_t>(temp);
}

}  // namespace

// 0x10237304: builds "<users path with theme><wide separator><restart name><.INTS>" and opens it; when the open
// fails the restart save is written through CFSLoadSave::SaveHidden, otherwise the file is closed again.
void CreateRestartParkSaveGame() {
    bfl::FileSystem::CDiscFile file;
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
    TbDynamicStringTemplate<char16_t> path = CopyUsersPathWithTheme();
    path += gTPGlobals.mWideSeparator;
    path += gTPGlobals.mRestartName;
    path += gTPGlobals.mF44AC;
    TbDynamicStringTemplate<char> name;
    UniStringToString(name, path);
    TbError result = file.Open(name.mData, 0x80000000u);
    if (static_cast<int32_t>(result) < 0)
        gLoadSave.SaveHidden(gTPGlobals.mRestartName);
    else
        file.Close();
}
