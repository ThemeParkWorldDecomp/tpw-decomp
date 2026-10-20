// FS_MasterSave.cpp range of the main app (SimTheme Park 2.04b3). PARTIAL: only
// FS_ScanThemes (0x10169310) so far; CFSGlobalMasterSave is still to do.
#include "FS_MasterSave.h"

#include "FS_System.h"
#include "Game.h"
#include "TPGlobals.h"
#include "bfl_FileSystem.h"

// Every folder inside <localPath>:data:levels is a theme: its name goes to the game. The
// result is 0 once the folder could be searched at all, -1 otherwise.
TbError FS_ScanThemes(const TbDynamicStringTemplate<char>& localPath) {
    bfl::FileSystem::CDiscFileStorage storage;
    bfl::FileSystem::CFileInfo info;
    TbDynamicStringTemplate<char> path;
    path = localPath;
    FS_AddDirectorySeparator(path);
    path += gTPGlobals.mF4088;
    int32_t result = -1;
    if (storage.Open(path.mData).Succeeded()) {
        if (storage.FindFirst(gTPGlobals.mAllFiles.mData, info).Succeeded()) {
            result = 0;
            do {
                if (FS_IsSubDirectory(info)) gGame.AddThemeName(TbDynamicStringTemplate<char>(info.mName));
            } while (storage.FindNext(info).Succeeded());
        }
    }
    return TbError(result);
}
