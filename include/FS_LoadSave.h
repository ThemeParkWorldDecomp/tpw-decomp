// FS_LoadSave.cpp (main app): park saving and loading by name. PARTIAL: LoadMostRecent is
// translated (src/game/save/FS_LoadSave.cpp, unit w108); the others are not.
#pragma once
#include <cstdint>

#include "TbDynamicStringTemplate.h"
#include "TbTList.h"

struct TFSLoadSaveSaveFileDetails;   // unit v5183: the details of one save file (below the class)
// Unit v5958: the object is a list of the save file details (the Mac's four words: table, head, tail, count; 0x10 bytes).
class CFSLoadSave : public TbTList<TFSLoadSaveSaveFileDetails*, TFSLoadSaveSaveFileDetails*> {
public:
    // Unit v5958 (src/game/save/FS_LoadSave.cpp): 0x101fc790 (384 bytes): saves the park
    // as "<the player's folder for the theme>:<name><extension>" through CSaveLoad::Save (no header, type 2); its answer.
    int32_t SaveInternal(const TbDynamicStringTemplate<char16_t>& name, const TbDynamicStringTemplate<char16_t>& extension);
    // Unit v5183: the save file list. 0x101fc97c (848 bytes) the first entry or NULL; 0x101fcd08 (108 bytes) the one after `previous` or NULL.
    TFSLoadSaveSaveFileDetails* GetFirstSaveFileDetails();
    TFSLoadSaveSaveFileDetails* GetNextSaveFileDetails(const TFSLoadSaveSaveFileDetails* previous);
    void LoadHidden(const TbDynamicStringTemplate<char16_t>& name);   // 0x101fc47c
    void SaveHidden(const TbDynamicStringTemplate<char16_t>& name);   // 0x101fc568
    void Save(const TbDynamicStringTemplate<char16_t>& name);         // 0x101fc4f4
    // 0x101fcdcc (816 bytes): loads the newest park file of the logged-in player's folder for the current theme (the files that
    // match the save pattern, no sub folders; the newest last write time wins, the first of equals wins) through CSaveLoad::Load,
    // type 2, no header. Nothing happens when the folder or a file is not there.
    void LoadMostRecent();
    // 0x101fc408 (44 bytes, src/game/save/FS_LoadSave.cpp, unit w117): LoadInternal with the extension ".TPWS" (gTPGlobals.mF4420); its answer.
    int32_t Load(const TbDynamicStringTemplate<char16_t>& name);
    // 0x101fc5e0 (324 bytes, same file): loads the park file "<the logged-in player's folder for the theme>:<name><extension>" through
    // CSaveLoad::Load (no header, type 2); 1 on success, else 0.
    int32_t LoadInternal(const TbDynamicStringTemplate<char16_t>& name, const TbDynamicStringTemplate<char16_t>& extension);
    // Unit v51056: ~CFSLoadSave 0x101fc374 with CodeWarrior's hidden flag (r4, a short). Nothing for NULL; otherwise the list's destructor
    // without deleting, then the storage is deleted when the flag is above 0. Answers this. (The Mac also stores the table word
    // 0x102b1540 at +0 first; the host object keeps its own vptr there, so that store is not made.)
    static CFSLoadSave* Destroy(CFSLoadSave* self, int16_t flag);
};
// Unit v5183: an entry of the save file list. Only the start is known: the name is a string object (Mac vptr +0, length +4, text pointer +8).
struct TFSLoadSaveSaveFileDetails {
    TbDynamicStringTemplate<char16_t> mName;   // 0x00
    // Unit v5958 (the Mac object is 0x1c bytes: the name is 0x14 of them): the two words of the directory entry at 0x08 (the file's stamp).
    uint32_t mStampHigh;                       // Mac 0x14
    uint32_t mStampLow;                        // Mac 0x18
};
extern CFSLoadSave gLoadSave;   // 0x10396a7c, original name unknown
// Unit v51056: TbTList<TFSLoadSaveSaveFileDetails*, ...>::~TbTList 0x101fd130 called directly (not through the vtable) with the flag 0.
namespace v51056_callee {
void LoadSaveListDtor(CFSLoadSave* self);
}
// Unit v5720 (src/game/save/FS_LoadSave.cpp): 0x101fd668 (84 bytes) the static initialiser of
// FS_LoadSave.cpp: gLoadSave's four words (0x10 bytes on the Mac: word 0 the 0x102b1540 table word, 1 to 3 zero) and its
// __register_global_object record.
void __sinit_FS_LoadSave_cpp();
extern uint32_t gLoadSaveWords[4];        // 0x10396a7c (the words of gLoadSave, 16 bytes)
extern uint8_t gLoadSaveNode[12];         // 0x10396a8c (the registration record)
extern char gLoadSaveDtorVector[8];       // stand-in for the destructor TVector word 0x10268660 (~CFSLoadSave 0x101fc374)
