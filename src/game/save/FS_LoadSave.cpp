// FS_LoadSave.cpp: name proven by __sinit_FS_LoadSave_cpp in the main application.

#include "AdvisorScoring.h"
#include "bfl_FileSystem.h"
#include "email.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "GameSys.h"
#include "InterfaceWindow.h"
#include "keyboard.h"
#include "LbDrawPrimitives.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "lobby.h"
#include "online.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "SaveLoad.h"
#include "tb_ezsprite.h"
#include "tb_starfield.h"
#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_menu.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// CFSLoadSave::Load and LoadInternal (FS_LoadSave.cpp, main app 0x101fc408 and 0x101fc5e0, SimTheme Park 2.04b3; unit w117). A file of its own so
// that the test of LoadMostRecent (src/FS_LoadSave.cpp) does not have to link them.

// Ghidra says void; the listing returns what LoadInternal answered.
int32_t CFSLoadSave::Load(const TbDynamicStringTemplate<char16_t>& name) { return LoadInternal(name, gTPGlobals.mF4420); }

// Unit v51056: CUIGrapher's setters (0x101fd798 .. 0x101fd97c), CFSLoadSave's Save/Hidden wrappers and destructor, the ui_mainmenu.cpp
// small functions and static initialiser, the TPEmbed.cpp small functions and the bthread / Connection / vector destructors.

// ---- CFSLoadSave
void CFSLoadSave::LoadHidden(const TbDynamicStringTemplate<char16_t>& name) { LoadInternal(name, gTPGlobals.mF44AC); }
void CFSLoadSave::Save(const TbDynamicStringTemplate<char16_t>& name) { SaveInternal(name, gTPGlobals.mF4420); }
void CFSLoadSave::SaveHidden(const TbDynamicStringTemplate<char16_t>& name) { SaveInternal(name, gTPGlobals.mF44AC); }

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

// Unit v5958: the main menu's quit answer, the main menu item callback and its fade, and two of CFSLoadSave's functions (the save
// of a park by name, the first entry of the list of the player's save files).

namespace {
using WideString = TbDynamicStringTemplate<char16_t>;
using NarrowString = TbDynamicStringTemplate<char>;
}  // namespace

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

// Unit v51047: std::basic_istream<char>::basic_istream 0x1020a4c0, CUIGrapher::Clear 0x101fd9d0 and its constructor 0x101fd6e8,
// CFSLoadSave::GetNextSaveFileDetails 0x101fcd08, CUI2DMapSprite::~CUI2DMapSprite 0x10230ea4, SetVisibility 0x10230f78 and SetScale
// 0x10231094, the program entry 0x102381a4 and InterfaceKnob::operator- 0x101d038c.

void CUIGrapher_Construct(CUIGrapher* self) {
    for (int i = 0; i < 5; i++) {
        self->mXAxis[i] = 0;
        self->mYAxis[i] = 0;
        self->mGrid[i] = 0;
        self->mBG[i] = 0;
    }
    self->mF00 = 0;
    self->mF04 = 0;
    self->mF2C = 0;
    self->mF28 = 0;
    self->mF24 = 0;
    self->mF20 = 0;
    self->mF1C = 0;
    self->mF18 = 0;
    self->mF14 = 0;
    self->mF10 = 0;
    self->mF0C = 0;
    self->mF08 = 0;
}

// ---- CFSLoadSave::GetNextSaveFileDetails 0x101fcd08: the entry AFTER the first one equal to `previous` (the node after the match is
// read before the comparison), NULL when the match is the last or there is none.
TFSLoadSaveSaveFileDetails* CFSLoadSave::GetNextSaveFileDetails(const TFSLoadSaveSaveFileDetails* previous) {
    CNode* node = mHead;
    while (node != nullptr) {
        TFSLoadSaveSaveFileDetails* data = node->mData;
        node = node->mNext;
        if (TPUniStrEq(previous->mName, data->mName)) break;
    }
    if (node == nullptr) return nullptr;
    return node->mData;
}

// FS_LoadSave.cpp (main app 0x101fc47c..0x101fd0fc, SimTheme Park 2.04b3). PARTIAL: only
// LoadMostRecent (0x101fcdcc) so far; LoadHidden, Save and SaveHidden are still to do.

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

extern "C" void __register_global_object(void* object, void* destructor, void* node);

// gLoadSave's 16 bytes (0x10396a7c), the registration record (0x10396a8c) and the destructor stand-in.
uint32_t gLoadSaveWords[4];
uint8_t gLoadSaveNode[12];
char gLoadSaveDtorVector[8];

// 0x101fd668 (84 bytes): stores the 0x102b154c table word, zeroes words 1 to 3, overwrites word 0 with the 0x102b1540 table word
// (both stores are in the original) and registers the object for destruction.
void __sinit_FS_LoadSave_cpp() {
    gLoadSaveWords[0] = 0x102b154cu;
    gLoadSaveWords[1] = 0;
    gLoadSaveWords[2] = 0;
    gLoadSaveWords[3] = 0;
    gLoadSaveWords[0] = 0x102b1540u;
    __register_global_object(gLoadSaveWords, gLoadSaveDtorVector, gLoadSaveNode);
}
