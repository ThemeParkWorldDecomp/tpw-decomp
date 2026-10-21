// ESP_sprites.h: the ESP sprite loader of HLSprite.cpp (main app, 0x1007ace0..0x1007c93c and ESP_GetParticleSprite
// 0x1007cefc; src/game/HLSprite.cpp). An .ESP file describes one people sprite set: the name of its data file (.tps/.tpc
// for the third person, .fps/.fpc for the first person, which is what the game ships as ".TPC" and ".FPC"), sixteen
// action records and, in the later files, four blocks of custom data. ESP_FindESPFiles lists the .ESP files of a
// sprite folder (the "generic" one, then the current theme's), sorts them by name and loads them; the sprites go into
// gESPSprites, in the order of ESP_LoadSprites' folders, and the sprite set of a folder is gESPGroups[n].
//
// PARTIAL: the rest of the HLSprite.cpp range (ESP_RemoveSprites, the getters, ESP_AddHLSpriteFromScript, the first
// and third person switches, the flies) is not translated; HLSprite.h declares it. ESP_InitialiseAndLoadSprites is
// declared there.
#pragma once
#include <cstddef>
#include <cstdint>

#include "TPResource.h"
#include "bfl_FileSystem.h"
#include "sprite_engine.h"

class CThing;

// The action state type of the sprite scripts (mangled "4ACTS"). Only its size is known: a 32-bit value.
enum ACTS : int32_t {};
// Unit w143: the sprite set numbers the staff modes spawn scripts for (4 to 8); a 32-bit value.
enum ESP_Sprites : int32_t {};
// 0x100da18c (332 bytes, src/game/Script.cpp, unit w163; mangled __FPv11ESP_Spritesi4ACTSfff): starts a script on a sprite; answers the script handle.
int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z);

struct PTCLTexture;

// One sprite set (0x260 bytes on the Mac; the first pointer makes the host's four bytes larger and every later field
// moves by those four). Made with operator new and ESP_CSprite::ESP_CSprite, which does not clear everything: the
// name, mF205, mF206 and the bytes 0x20f, 0x213, 0x217 and 0x21b of the custom data keep what the heap held.
class ESP_CSprite {
public:
    // One of the sixteen actions: the sprite file's first record of the action, how many records an animation of it has
    // and whether it comes in several views. The file holds them in this order.
    struct Action {
        uint16_t mFirstFrame;   // +0  an index into the sprite file's records
        uint8_t mFrames;        // +2  records in one view; 0: the set has no such action
        uint8_t mViews;         // +3  non-zero: the action has eight views, a view is `variant & 7` times mFrames on
    };

    ESP_CSprite();   // 0x1007b150

    // 0x1007b1f8: the number of records in one view of action `action` (0 to 15; no check).
    uint8_t GetActionFrames(int32_t action) const;
    // 0x1007b23c: the custom data block `block` (0 to 3, anything else does nothing): *first = byte 0 - 1,
    // *second = byte 1 - 1 (a 32-bit value), *third = byte 2, *fourth = byte 3. A file without the blocks has
    // zeros (so -1 comes out), a sprite never loaded has what the heap held in bytes 1 of the blocks.
    void GetCustomData(int32_t block, int32_t* first, ACTS* second, int32_t* third, int32_t* fourth) const;
    // 0x1007b354: reads the .ESP file `info` names from `storage`, then its data file (the same name with the
    // extension of the sprite kind) through CSpriteFileIO::Load. 0 when the first file does not open, when the
    // object cannot be made or when the second does not open; 1 otherwise, even when Load fails (mLoaded then stays
    // 0). The file object is never deleted (a leak of the original). Counts the sprite in gESPSpriteCount.
    bool LoadFromESPFile(bfl::FileSystem::CFileInfo& info, bfl::FileSystem::CFileStorage& storage);

    CSpriteFileIO* mFile;           // 0x000  the sprite data file; NULL until LoadFromESPFile makes it
    char mName[0x200];              // 0x004  the .ESP file's name without the extension
    uint8_t mLoaded;                // 0x204  CSpriteFileIO::Load succeeded
    uint8_t mHasFirstPerson;        // 0x205  from the file: the set has first person sprites
    uint8_t mF206;                  // 0x206  from the file
    uint8_t mSemitransparent;       // 0x207  1 after the constructor; LoadFromESPFile copies gESPSemitransparent
    uint32_t mRecordCount;          // 0x208  the data file's record count (CSpriteFileIO's first word)
    uint16_t mCustomCount;          // 0x20c  how many of the four custom blocks have a non-zero first byte
    uint8_t mCustom[16];            // 0x20e  four custom blocks of four bytes (see GetCustomData)
    uint16_t mActionCount;          // 0x21e  how many of the actions have mFrames != 0
    Action mActions[16];            // 0x220
};
static_assert(offsetof(ESP_CSprite, mActions) == 0x220 + sizeof(void*) - 4 && sizeof(ESP_CSprite::Action) == 4,
              "ESP_CSprite keeps the Mac's layout after the pointer (0x260 bytes there)");

// One ESP file found by ESP_FindESPFiles (12 bytes on the Mac). The directory entry and the storage it was found in
// are copies, owned by the list.
struct ESPFileEntry {
    bfl::FileSystem::CFileInfo* mInfo;          // +0
    bfl::FileSystem::CFileStorage* mStorage;    // +4
    uint8_t mLoaded;                            // +8  a sprite was made from it
};

// One folder of sprites (12 bytes on the Mac): the folder's name, the first sprite it made and how many.
struct ESPSpriteGroup {
    const char* mName;     // +0
    int32_t mFirst;        // +4  index into gESPSprites
    int32_t mCount;        // +8
};

enum : int32_t { kESPMaxFiles = 0x80, kESPMaxSprites = 0x200, kESPGroups = 14 };

// ---- data (main app; the names are invented, the Mac addresses are in the comments)
extern ESPFileEntry gESPFiles[kESPMaxFiles];    // 0x102dabe0  the files of the folder being loaded
extern int32_t gESPFileCount;                   // 0x102dabdc
extern uint8_t gESPSemitransparent;             // 0x102db1e0  set to 1 by ESP_FindESPFiles
extern int32_t gESPSpriteCount;                 // 0x102db1e4  sprites made by the current ESP_LoadSprites
extern int32_t gESPTotalSprites;                // 0x102db1e8  gESPSpriteCount when ESP_LoadSprites ended
// 0x102db1ec (0x200 bytes): ESP_FindESPFiles builds "generic:<folder>:" and "<theme>:<folder>:" here and never reads
// them again.
extern char gESPPathBuffer[0x200];
extern bfl::FileSystem::CFileInfo gESPFileInfo;     // 0x102db3ec  the search's current entry
// 0x102db520: the sprites. The original's table has room for 0x200 and ESP_FindESPFiles lets the 0x201st through
// (it tests the count after the store); the host table has a margin of 16.
extern ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
extern ESPSpriteGroup gESPGroups[kESPGroups];       // 0x1029d368  kids, kidsheads, costumes, costumeheads, entertainers,
                                                    //             handymen, mechanics, guards, researchers, thoughts,
                                                    //             balloons, litter, specialfx, particles
extern const char* gESPKidFiles[4];                 // 0x1029d410  "SPR_BI.ESP", "SPR_KI.ESP", "SPR_TA.ESP", "SPR_SU.ESP"
extern uint8_t gESPCompressed;                      // 0x1029d310  1 at start: the sprites come from .tpc/.fpc files
extern int32_t gESPFirstPerson;                     // 0x102dbd3c  ESP_InitialiseAndLoadSprites' first argument
extern int32_t gESPThirdPerson;                     // 0x1029d360  1 - that
extern uint8_t gESPDrawEnabled;                     // 0x1029d364  1 at start; ESP_EnableSpriteDraw/DisableSpriteDraw
// 0x102dbd24..0x102dbd34 and the rest ESP_InitialiseAndLoadSprites sets
extern int32_t gESPCellsHigh;                       // 0x102dbd24  (map height / 4) + 1
extern int32_t gESPCellsWide;                       // 0x102dbd28  (map width / 4) + 1
extern int32_t gESPMapHeight;                       // 0x102dbd2c  the height field's +0x1c
extern int32_t gESPMapWidth;                        // 0x102dbd30  the height field's +0x18
extern int32_t gESPF34;                             // 0x102dbd34  cleared after loading
extern CThing* gESPF20;                             // 0x102dbd20  cleared first; the pointed peep of the last frame (a word on the Mac)
extern float gESPF88;                               // 0x102dc088  cleared first
extern SSpriteGameData** gESPListInstances[4][4];   // 0x102dbdbc  what CSpriteList::Init made for each list
extern SSpriteGameData** gESPGlobalInstances;       // 0x102dbd44  the same for gESPGlobalList
extern CSpriteList gESPGlobalList;                  // 0x102dbd48  linked as map list 0x20, not into the cells

// ---- the functions
// 0x1007ba5c: sorts gESPFiles by file name, ignoring case (a bubble sort: equal names keep their order).
void ESP_SortFileList();
// 0x1007bb80: lists the .ESP files of folder `name` of the sprite resource (first "generic:<name>:*.ESP", then the
// current theme's; at most 0x80), sorts them and makes a sprite of each one that loads. The kids folders ("kids",
// "kidsheads") stop at COptions::GetNumKidSprites sprites and make the four preferred files (gESPKidFiles) first; the
// staff folders (handymen, mechanics, guards, researchers) stop at GetNumStaffSprites; every folder stops at 0x200
// sprites in all. Answers how many sprites it made.
int32_t ESP_FindESPFiles(CTPSpriteResource& resource, const TbDynamicStringTemplate<char>& name);
// 0x1007c63c: looks for the "Sprites" resource and loads its folders: only "particles" when `particlesOnly`, else all
// fourteen. Fills gESPGroups' first sprite and count. 1 when the resource exists, else 0.
bool ESP_LoadSprites(bool particlesOnly);
// 0x1007cefc: the record of particle sprite `sprite` (the folder is the particles', the set is sprite / 16, the action
// sprite % 16) at frame `frame`; `variant` & 7 picks the view for an action that has views. The set's first record
// when the index falls outside the file.
PTCLTexture* ESP_GetParticleSprite(int32_t sprite, int32_t frame, int32_t variant);
// Unit w141 (untranslated): 0x1007cfc8 and 0x1007d280, switch the sprites between the first person and the third person view.
// (Unit v5027, src/game/HLSprite.cpp: both answer r3: 1 when they reloaded the sprites, 0 when the view was
// already set or the "Sprites" resource is missing.)
void ESP_SwitchToFirstPersonSprites();
void ESP_SwitchToThirdPersonSprites();
int32_t ESP_SwitchSprites(int32_t third);   // the shared body (0 first person, 1 third person); returns the r3 the originals answer
// Unit w194 (untranslated): 0x1007aca4 (16 bytes).
void ESP_DisableSpriteDraw();
// Unit w207 (untranslated): 0x1007ac68 (16 bytes), the pair of ESP_DisableSpriteDraw; CWorld::Load calls it for every world state but 4.
void ESP_EnableSpriteDraw();

// Unit v5027 (src/game/HLSprite.cpp), used by ESP_AddHLSpriteFromScript (names invented).
extern int32_t gESPMaxCellX;        // 0x1029d420  3
extern int32_t gESPMinCellX;        // 0x1029d424  0
extern int32_t gESPMaxCellZ;        // 0x1029d428  3
extern int32_t gESPMinCellZ;        // 0x1029d42c  0
extern int32_t gESPGlobalCount;     // 0x102dbd40  how many entries gESPGlobalInstances holds
extern int32_t gESPCellCounts[4][4];   // 0x102dbd7c  how many sprites each cell's list holds (the list is full at 0x800)

// Unit v51046 (src/game/HLSprite.cpp)
// 0x1007c9f8: a random number below the action count of sprite `sprites` of the set (gESPSprites[gESPGroups[set].mFirst + sprites]):
// (GetRand() >> 2) mod mActionCount, unsigned. No range check; an action count of 0 divides by zero on the Mac (answers GetRand() >> 2
// here, as the emulator does).
int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites);
