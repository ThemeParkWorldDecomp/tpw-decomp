// Unit v5958: the main menu's quit answer, the main menu item callback and its fade, and two of CFSLoadSave's functions (the save
// of a park by name, the first entry of the list of the player's save files).
#include <cstdlib>
#include <cstring>
#include <new>
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "InterfaceWindow.h"
#include "SaveLoad.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "World.h"
#include "bfl_FileSystem.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_publishParkPanel.h"

namespace {
using WideString = TbDynamicStringTemplate<char16_t>;
using NarrowString = TbDynamicStringTemplate<char>;
}  // namespace

// 0x101fab10 (148): the quit answer of the game menu's confirm box: the online system (made when there is none) stops its jobs, the game
// goes to the quit state and the main menu, when there is one, is hidden. Answers 1.
int32_t MainGameMenuConfirmQuit_OK(void* context) {
    (void)context;
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();   // 0x404 bytes on the Mac
    gOnlineSystem->StopProcessing();
    GS_SetCurrentGameStatus(GS_QUIT_GAME);
    if (gMainMenu != nullptr) HideMainMenu();
    return 1;
}

// 0x101fa8d4 (212): extra parameter 2 is the fade: above 0 the item brightens by 0x20 a frame (grey levels, past 0xff it ends at white
// and the parameter is 0), below 0 it steps down by 0x20 (to 0 once it is below -0xdf) and the item takes its own colour.
void itemOnFrame(InterfaceWindow* window) {
    int32_t fade = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(2)));
    if (fade == 0) return;
    if (fade > 0) {
        int32_t next = fade + 0x20;
        int32_t level = next;
        if (next > 0xff) {
            level = 0xff;
            next = 0;
        }
        window->SetFontColour(level, level, level, 0xff);
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(next)));
    } else {
        int32_t next = fade - 0x20;
        if (fade + 0xdf < 0) next = 0;
        window->SetFontColour(0, 0xaf, 0xbe, 0xff);
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(next)));
    }
}

// 0x101fa9dc (248): the main menu item handler: message 0x1e (a frame) runs the fade, 0x11 sets the item colour, 0x10001 and 0x10002
// start the fade up (1) and down (-1); then the default item handler answers.
int32_t MainMenuItemCB(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1e) {
        itemOnFrame(window);
    } else if (message < 0x1e) {
        if (message == 0x11) window->SetFontColour(0, 0xaf, 0xbe, 0xff);
    } else if (message == 0x10002) {
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
    } else if (message < 0x10002 && message > 0x10000) {
        window->setExtraParam(2, reinterpret_cast<void*>(static_cast<intptr_t>(1)));
    }
    return UIMenu_DefaultItemCallback(window, message, a, b);
}

// 0x101fc790 (384): the park file is "<the player's folder for the theme><separator><name><extension>"; the global master save is
// written first.
int32_t CFSLoadSave::SaveInternal(const TbDynamicStringTemplate<char16_t>& name, const TbDynamicStringTemplate<char16_t>& extension) {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;   // 0xd4 bytes on the Mac
    gUserInfo->SaveGlobalMasterSave();
    WideString path;
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    {
        WideString folder = gUserInfo->ReturnUsersPathWithTheme();
        path = folder;
    }
    path += gTPGlobals.mWideSeparator;
    path += name;
    path += extension;
    NarrowString narrow;
    UniStringToString(narrow, path);
    return gSaveLoad.Save(narrow.mData, nullptr, static_cast<CSaveLoad::LoadType>(2));
}

// 0x101fc97c (848): empties the list (the details are destroyed and freed), then lists the files of the player's folder that match
// "<all files><save extension>" and are no sub folders: each as details holding the name without the extension and the entry's two stamp
// words, added at the head when the list is empty, else at the tail when the details are not in the list already. Answers the first
// details, NULL when there are none.
TFSLoadSaveSaveFileDetails* CFSLoadSave::GetFirstSaveFileDetails() {
    while (mCount != 0) {
        TFSLoadSaveSaveFileDetails* old = RemoveHead();
        if (old != nullptr) {
            old->mName.~WideString();
            ::operator delete(old);
        }
    }
    RemoveAll();
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    // (the original builds the copy from the returned string and destroys the returned one)
    WideString usersPath = [&]() {
        WideString returned = gUserInfo->ReturnUsersPathWithTheme();
        return WideString(returned);
    }();
    NarrowString folder;
    UniStringToString(folder, usersPath);
    bfl::FileSystem::CDiscFileStorage storage;
    auto opened = storage.Open(folder.mData);
    if (opened.mValue >= 0) {
        bfl::FileSystem::CFileInfo info;
        NarrowString pattern;
        pattern = gTPGlobals.mAllFiles;
        pattern += gTPGlobals.mSaveExtension;
        auto found = storage.FindFirst(pattern.mData, info);
        while (found.mValue >= 0) {
            if (!FS_IsSubDirectory(info)) {
                NarrowString fileName(info.mName);
                int32_t nameLength = fileName.Length();
                int32_t extensionLength = gTPGlobals.mSaveExtension.Length();
                fileName = fileName.Left(static_cast<uint32_t>(nameLength - extensionLength));
                TFSLoadSaveSaveFileDetails* details = new TFSLoadSaveSaveFileDetails;   // 0x1c bytes on the Mac
                StringToUniString(details->mName, fileName);
                std::memcpy(&details->mStampHigh, info.mF08, 4);
                std::memcpy(&details->mStampLow, info.mF08 + 4, 4);
                if (mCount == 0) {
                    AddHead(details);
                } else if (Find(details) == nullptr) {
                    AddTail(details);
                }
            }
            found = storage.FindNext(info);
        }
    }
    if (mCount == 0) return nullptr;
    return mHead->mData;
}
