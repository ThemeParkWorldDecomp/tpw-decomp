// FS_LoadSave.cpp (main app 0x101fc47c..0x101fd0fc, SimTheme Park 2.04b3). PARTIAL: only
// LoadMostRecent (0x101fcdcc) so far; LoadHidden, Save and SaveHidden are still to do.
#include "FS_LoadSave.h"

#include "FS_System.h"
#include "FS_UserInfo.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "bfl_FileSystem.h"

// Scans the logged-in player's folder for the current theme for the files that match "*" + ".TPW*"
// and loads the newest one. The folder is made into a narrow string first (the library works with
// narrow paths). Every entry that is not a sub folder competes on its last write time: the first
// one is kept, and a later one replaces it only when LbFile_CompareFileStamps says it is newer (a
// positive answer), so of equal times the first wins. The result of Load is ignored.
void CFSLoadSave::LoadMostRecent() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    // (the original copies the result and destroys the temporary; unrecorded either way)
    const TbDynamicStringTemplate<char16_t> usersPath(gUserInfo->ReturnUsersPathWithTheme());
    TbDynamicStringTemplate<char> folder;
    UniStringToString(folder, usersPath);
    bfl::FileSystem::CDiscFileStorage storage;
    if (!storage.Open(folder.mData).Succeeded()) return;

    bfl::FileSystem::CFileInfo info;
    TbDynamicStringTemplate<char> pattern;
    pattern = gTPGlobals.mAllFiles;
    pattern += gTPGlobals.mSaveFilePattern;
    if (!storage.FindFirst(pattern.mData, info).Succeeded()) return;

    // the newest entry so far: the whole entry (its name pointer rebased into its own buffer)
    bool haveBest = false;
    bfl::FileSystem::CFileInfo best;
    do {
        if (!FS_IsSubDirectory(info)) {
            if (!haveBest) {
                best = info;
                haveBest = true;
            } else if (LbFile_CompareFileStamps(reinterpret_cast<const TbFileStamp*>(info.mF08),
                                                reinterpret_cast<const TbFileStamp*>(best.mF08)) > 0) {
                best = info;
            }
        }
    } while (storage.FindNext(info).Succeeded());

    if (haveBest) {
        TbDynamicStringTemplate<char> path(folder);
        path += gTPGlobals.mSeparator;
        path += best.mName;
        gSaveLoad.Load(path.mData, nullptr, CSaveLoad::kLoadPark);
    }
}
