// HLSprite.cpp: name proven by __sinit_HLSprite_cpp in the main application.

#define TPW_GLOBE_INIT_REAL 1
#define TPW_GLOBE_SHOWHIDE_REAL   // the download globe's Show calls the online globe's real Show (0x100ad788), not the inline base
#include "AdvisorQueue.h"
#include "analyser.h"
#include "Bitmap.h"
#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Clock.h"
#include "Debug.h"
#include "effect_manager.h"
#include "email.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "Game.h"
#include "helpers/HLSpriteHelpers.h"
#include "HLSprite.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "Light.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "MapWho.h"
#include "ObjectParts.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "sprite_engine.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "tb_hfield.h"
#include "tb_particle.h"
#include "tb_printf_opengl.h"
#include "tb_rain.h"
#include "tb_resource_display.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPOptions.h"
#include "TPResource.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TPString.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// Unit v5209: FLY_Load 0x10079aa8, FLY_RestoreMeshes 0x10079f8c, OBJECT_Load 0x1007a0b8 (the HLSprite.cpp range).
//
// The Mac swaps every word it reads from a file (SamsUtilities::_USwapBlock32, USwapInplace32<long>, tag_IDX_TYPE::Byteswap,
// USwapBlockStruct<FLYBOY>): the files are little-endian, so on this little-endian host there is nothing to do and the swaps are
// left out (the same as the particle loader's helpers in rsse.h).

namespace {

// The original's x / 1024 (srawi, addze: rounds toward zero).
int32_t Div1024(int32_t value) { return value / 1024; }

}  // namespace

// 0x1007a0b8: loads the level's object file (a NULL path does nothing).
void OBJECT_Load(char* path) {
    if (path == nullptr) return;
    void* file = nullptr;
    LbFile_Open(file, path, 0x80000000u);
    if (file == nullptr) return;
    // This file is the Mac's own (big-endian) and the original reads its words as they are: on the little-endian host every word
    // read is byte-reversed to give the value the Mac saw.
    uint32_t magic;
    int32_t count;
    uint32_t recordSize;
    LbFile_Read(file, &magic, 4, nullptr);
    if (__builtin_bswap32(magic) != 0x5f4a424fu) return;
    LbFile_Read(file, &count, 4, nullptr);
    count = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(count)));
    LbFile_Read(file, &recordSize, 4, nullptr);
    recordSize = __builtin_bswap32(recordSize);
    for (int32_t n = 0; n < count; ++n) {
        int32_t rec[15];
        std::memset(rec, 0, sizeof(rec));
        if (recordSize > 0x3c) {
            LbFile_Read(file, rec, 0x3c, nullptr);
            LbFile_Seek(file, static_cast<int32_t>(recordSize - 0x3c), 1);
        } else {
            LbFile_Read(file, rec, recordSize, nullptr);
        }
        for (int32_t& word : rec) word = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(word)));
        // rec[0] 0x58 type, [1] 0x5c bank flag, [2] 0x60 event, [3] 0x64 x, [4] 0x68 y, [5] 0x6c z, [6] 0x70, [7] 0x74, [8] 0x78
        switch (rec[0]) {
        case 1:
            SOUND_SendEventExt(0, rec[1] != 0 ? gSoundBanks[6] : gSoundBanks[0], rec[2], Div1024(rec[3]), Div1024(rec[4]),
                               Div1024(rec[5]), Div1024(static_cast<int32_t>(static_cast<uint32_t>(rec[6]) << 1)));
            break;
        case 2:
            PAR_InitEmitter(rec[1], rec[3], rec[4], rec[5]);
            break;
        case 3: {
            int32_t handle = SOUND_SendEvent(0, rec[1] != 0 ? gSoundBanks[6] : gSoundBanks[0], rec[2], Div1024(rec[3]),
                                             Div1024(rec[4]), Div1024(rec[5]));
            SOUND_SetRect(handle, Div1024(rec[3]), Div1024(rec[5]), Div1024(rec[7]), Div1024(rec[8]));
            break;
        }
        default:
            break;
        }
    }
    LbFile_Close(file);
}

// The effect manager (main app of SimTheme Park 2.04b3, the HLSprite.cpp range of the inventory, 0x1007a3d0..0x1007a6b8): EM_Init,
// EM_SetEmitersOn, EM_SetEffectTranslation, EM_SetEffectOrbit and EM_Tick. The advisor's sparkles are effects: a few emitters of the
// particle library that circle a point which the advisor moves about the screen. See include/effect_manager.h.

EMEffect gEffects[kEffects];   // 0x102dab10

namespace {

// The constants of the main app's data (0x10264be0..0x10264bf0, 0x1029d304).
constexpr float kOne = 1.0f;               // 0x10264be0, and the limit EM_Tick stops the radii's progress at (0x1029d304)
constexpr float kSineScale = 0.0009765625f;   // 0x10264bd4: 2^-10, the table's sines are 1024 times the sine
constexpr float kScreenWidth = 50000.0f;   // 0x10264bd0
constexpr float kScreenDepth = 37500.0f;   // 0x10264bcc
constexpr double kTable = 512.0;           // 0x10264bd8: the table's entries make a turn

// The listing builds a float from a signed integer by subtracting the magic double 2^52 + 2^31 from the integer's bits under 0x43300000,
// then rounding to single: the nearest float to the integer.
inline float ToFloat(int32_t value) { return static_cast<float>(value); }

// The word of effect `effect`'s emitter list at `slot`: the original indexes past the five entries (into the next effect), so the
// table is addressed as bytes.
inline uint32_t& EmitterSlot(int32_t effect, int32_t slot) {
    return *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(&gEffects[0]) + effect * sizeof(EMEffect) +
                                        __builtin_offsetof(EMEffect, mEmitters) + slot * sizeof(uint32_t));
}

}  // namespace

// 0x1007a3d0
void EM_Init() {
    for (int32_t i = 0; i < kEffects; ++i) std::memset(&gEffects[i], 0, sizeof(EMEffect));
    BMP_GenerateSinTab();
}

// 0x1007a454. Emitters are started or ended one at a time, in the order of the list; a handle that is 0 (the effect never got one)
// is not ended. The count is stored last.
int32_t EM_SetEmitersOn(int32_t effect, int32_t count, int32_t emitter) {
    EMEffect& fx = *reinterpret_cast<EMEffect*>(reinterpret_cast<uint8_t*>(&gEffects[0]) + effect * sizeof(EMEffect));
    int32_t started = 0;
    if (fx.mCount < count) {
        for (int32_t slot = fx.mCount; slot < count; ++slot) {
            EmitterSlot(effect, slot) = PAR_InitEmitter(emitter, 0, 0, 0);
            ++started;
        }
    } else if (count < fx.mCount) {
        for (int32_t slot = count; slot < fx.mCount; ++slot) {
            uint32_t& handle = EmitterSlot(effect, slot);
            if (handle != 0) {
                PAR_KillEmitter(static_cast<int32_t>(handle));
                --started;
                handle = 0;
            }
        }
    }
    fx.mCount = count;
    return started;
}

// 0x1007a558. The move runs from the old target to the new one when `index` is 0, from the new one to itself otherwise (no
// move). Only a duration other than 0 starts the clock (the start and duration are left alone for 0).
void EM_SetEffectTranslation(int32_t effect, int32_t index, float x, float y, float z) {
    EMEffect& fx = gEffects[effect];
    if (index == 0) {
        fx.mX = fx.mTargetX;
        fx.mY = fx.mTargetY;
    } else {
        fx.mX = x;
        fx.mY = y;
    }
    fx.mTargetX = x;
    fx.mTargetY = y;
    if (z != 0.0f) {
        fx.mStart = ToFloat(static_cast<int32_t>(RealTimePauseAdjusted()));
        fx.mDuration = z;
    }
}

// 0x1007a63c
void EM_SetEffectOrbit(int32_t effect, int32_t index, float a, float b, float c) {
    EMEffect& fx = gEffects[effect];
    if (index == 0) {
        fx.mOrbitX = fx.mOrbitTargetX;
        fx.mOrbitY = fx.mOrbitTargetY;
    } else {
        fx.mOrbitX = a;
        fx.mOrbitY = b;
    }
    fx.mOrbitTargetX = a;
    fx.mOrbitTargetY = b;
    fx.mOrbitTurns = c;
}

// 0x1007a6b8. `progress` is the move's elapsed time over its duration, in single precision (a duration of 0 gives infinity or NaN,
// which the orbit's clamp turns into 1 and the centre's move into whatever the float arithmetic gives). The centre follows the
// unclamped progress, the radii the clamped one. Emitter j of n stands j / n of a turn round the orbit plus progress * turns (the
// fraction of the sum, as a place in the table of 512 sines); its place is the centre plus (sin * radius x, -cos * radius y), put on
// a screen of 100000 by 75000 units, its height 0.
void EM_Tick() {
    for (int32_t i = 0; i < kEffects; ++i) {
        EMEffect& fx = gEffects[i];
        if (fx.mCount == 0) continue;
        const float now = ToFloat(static_cast<int32_t>(RealTimePauseAdjusted()));
        const float progress = (now - fx.mStart) / fx.mDuration;
        const float clamped = progress < kOne ? progress : kOne;
        const float centreX = ppc_fmadds(progress, fx.mTargetX - fx.mX, fx.mX);
        const float centreY = ppc_fmadds(progress, fx.mTargetY - fx.mY, fx.mY);
        for (int32_t j = 0; j < fx.mCount; ++j) {
            const float share = kOne / ToFloat(fx.mCount);
            const float turns = ppc_fmadds(ToFloat(j), share, progress * fx.mOrbitTurns);
            double whole;
            const float fraction = static_cast<float>(std::modf(static_cast<double>(turns), &whole));
            const int32_t angle = ppc_fctiwz(kTable * static_cast<double>(fraction));
            const float sine = ToFloat(HOST_Sin(angle)) * kSineScale;
            const float cosine = ToFloat(HOST_Cos(angle)) * kSineScale;
            const float radiusX = ppc_fmadds(clamped, fx.mOrbitTargetX - fx.mOrbitX, fx.mOrbitX);
            const float radiusY = ppc_fmadds(clamped, fx.mOrbitTargetY - fx.mOrbitY, fx.mOrbitY);
            const float x = centreX + sine * radiusX;
            const float y = centreY - cosine * radiusY;
            PAR_SetEmitterPos(static_cast<int32_t>(fx.mEmitters[j]), ppc_fctiwz(kScreenWidth * (kOne + x)), 0,
                              ppc_fctiwz(kScreenDepth * (kOne + y)));
        }
    }
}

// HLSprite.cpp, main app 0x1007a934..0x1007ac40: effect 2 follows a button.

float gHLBaseArea;
int8_t gHLBaseAreaSet;
float gHLAreaScale;
int8_t gHLAreaScaleSet;
int32_t gHLMinEmitters = 2;
int32_t gHLMaxEmitters = 5;

void BTN_HighLight_On(float x, float y, float width, float height) {
    if (gHLBaseAreaSet == 0) {
        gHLBaseArea = 0.0016f; // 0x10264bc8, 0x3ad1b717
        gHLBaseAreaSet = 1;
    }
    if (gHLAreaScaleSet == 0) {
        gHLAreaScale = 0.0051f; // 0x10264bc4, 0x3ba71de7
        gHLAreaScaleSet = 1;
    }
    const float area = width * height;
    const float scaled = (area - gHLBaseArea) / gHLAreaScale;
    const int32_t count = static_cast<int32_t>(static_cast<uint32_t>(ppc_fctiwz(3.0f * scaled)) + 2u);
    const int32_t clamped = std::max(gHLMinEmitters, std::min(count, gHLMaxEmitters));
    CTRACE(8, "%.3f %d", static_cast<double>(area), clamped);
    EM_SetEmitersOn(2, clamped, 0x23);
    EMEffect& effect = gEffects[2];
    effect.mX = x;
    effect.mY = y;
    effect.mTargetX = x;
    effect.mTargetY = y;
    // The conversion uses xoris, hence a signed clock word even though the clock returns unsigned.
    effect.mStart = static_cast<float>(static_cast<int32_t>(RealTimePauseAdjusted()));
    effect.mDuration = 400.0f; // 0x10264bbc
    effect.mOrbitX = effect.mOrbitTargetX = width / 3.0f;
    effect.mOrbitY = effect.mOrbitTargetY = height / 3.0f;
    effect.mOrbitTurns = 0.4f; // 0x10264bb8
    effect.mOrbitX = effect.mOrbitTargetX;
    effect.mOrbitY = effect.mOrbitTargetY;
    effect.mOrbitTargetX = width;
    effect.mOrbitTargetY = height;
    effect.mOrbitTurns = 0.4f;
}

void BTN_HighLight_Move(float x, float y) {
    gEffects[2].mX = x;
    gEffects[2].mY = y;
    gEffects[2].mTargetX = x;
    gEffects[2].mTargetY = y;
}

void BTN_HighLight_Kill() {
    for (int32_t i = 0; i < gEffects[2].mCount; ++i) {
        const uint32_t handle = gEffects[2].mEmitters[i];
        if (handle != 0) PAR_SetEmitterMask(handle, 1);
    }
    BTN_HighLight_Off();
}

void BTN_HighLight_Off() { EM_SetEmitersOn(2, 0, 0); }

// 0x1007ac68
void ESP_EnableSpriteDraw() { gESPDrawEnabled = 1; }
// 0x1007aca4
void ESP_DisableSpriteDraw() { gESPDrawEnabled = 0; }

// HLSprite.cpp: ResetHLSprites 0x1007af24 and DrawHLSprites 0x1007b02c.

int32_t gHLSpritesEnabled;   // 0x102dbd38

void ResetHLSprites() {
    for (int32_t i = 0; i < 4; i++)
        for (int32_t j = 0; j < 4; j++) gESPCellCounts[i][j] = 0;
    gESPGlobalCount = 0;
    gHLSpritesEnabled = 1;
    if (gFirstPersonFlags & 0x16) {
        gESPF88 = GetLMatrixRotationAroundYAxis(gCurrentCameraMatrix);
    } else {
        double d = std::fmod(static_cast<double>(gCameraF5BF4), 6.2831854820251465);
        gESPF88 = static_cast<float>(d);
        float v = gESPF88;
        if (!(v >= 0.0f)) gESPF88 = 6.2831855f + v;
    }
}

void DrawHLSprites() {
    if (gHLSpritesEnabled != 0) {
        for (int32_t i = 0; i < 4; i++)
            for (int32_t j = 0; j < 4; j++) gHLSpriteLists[i * 4 + j].mCount = gESPCellCounts[i][j];
        gESPGlobalList.mCount = gESPGlobalCount;
    }
    CThing* thing = LLCC_SystemGetPointedPeepThing();
    // the Mac keeps the thing as a 32-bit word in gESPF20; here it is a native pointer
    gIntrface.SetCurrentThing(gESPF20);
    gESPF20 = thing;
}

// 0x1007b114
void DrawHLSprite(tagSCRIPT* script) { script->mHasSprite = 1; }

// ESP_sprites.cpp: the ESP sprite loader (HLSprite.cpp of the main app, SimTheme Park 2.04b3): ESP_CSprite's
// constructor, GetActionFrames, GetCustomData and LoadFromESPFile (0x1007b150..0x1007b9e8), ESP_SortFileList
// (0x1007ba5c), ESP_FindESPFiles (0x1007bb80), ESP_LoadSprites (0x1007c63c), ESP_GetParticleSprite (0x1007cefc) and
// ESP_InitialiseAndLoadSprites (0x1007ace0, declared in HLSprite.h).
//
// THE .ESP FILE (little-endian, 334 bytes; the shipped files have 350, with the custom blocks):
//   0x000  8 bytes "ESP_FILE"        read and dropped
//   0x008  4 bytes "2.00"            read and dropped
//   0x00c  0x100 bytes               the data file's name ("SPR_BE.TPS", zero padded), read and dropped: the loader makes
//                                    the data file's name from the .ESP file's own
//   0x10c  1 byte                    mHasFirstPerson: the set also has first person sprites
//   0x10d  1 byte                    mF206
//   0x10e  16 actions of 4 bytes     u16 first record, u8 records in a view, u8 views flag
//   0x14e  16 bytes (optional)       four custom blocks of four bytes; the files before them end at 0x14e (the read of
//                                    the first byte answers 0 there and the other 15 are not read)
// The data file ("SPR_BE.TPC": the .ESP file's name with the extension replaced) is read by CSpriteFileIO::Load.

using bfl::FileSystem::CFile;
using bfl::FileSystem::CFileInfo;
using bfl::FileSystem::CFileStorage;

// ---- data
ESPFileEntry gESPFiles[kESPMaxFiles];
int32_t gESPFileCount;
uint8_t gESPSemitransparent;
int32_t gESPSpriteCount;
int32_t gESPTotalSprites;
char gESPPathBuffer[0x200];
CFileInfo gESPFileInfo;
ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
ESPSpriteGroup gESPGroups[kESPGroups] = {
    {"kids", 0, 0},         {"kidsheads", 0, 0},   {"costumes", 0, 0},  {"costumeheads", 0, 0}, {"entertainers", 0, 0},
    {"handymen", 0, 0},     {"mechanics", 0, 0},   {"guards", 0, 0},    {"researchers", 0, 0},  {"thoughts", 0, 0},
    {"balloons", 0, 0},     {"litter", 0, 0},      {"specialfx", 0, 0}, {"particles", 0, 0},
};
const char* gESPKidFiles[4] = {"SPR_BI.ESP", "SPR_KI.ESP", "SPR_TA.ESP", "SPR_SU.ESP"};
uint8_t gESPCompressed = 1;
int32_t gESPFirstPerson;
int32_t gESPThirdPerson = 1;
uint8_t gESPDrawEnabled = 1;
int32_t gESPCellsHigh;
int32_t gESPCellsWide;
int32_t gESPMapHeight;
int32_t gESPMapWidth;
int32_t gESPF34;
CThing* gESPF20;
float gESPF88;
SSpriteGameData** gESPListInstances[4][4];
SSpriteGameData** gESPGlobalInstances;
CSpriteList gESPGlobalList;
float gSpriteHotX;
float gSpriteHotY;

// ---- ESP_CSprite
namespace {

// Reads a little-endian 16-bit field of the file. The Mac reads the two bytes into the field, which is big-endian there,
// and swaps it in place (sthbrx), so a read that comes short, or fails, swaps what the field held (a fresh sprite's 0
// stays 0). Here the field's bytes are put in the Mac's order for the read and swapped by hand.
void ReadLittleEndian16(CFile& file, uint16_t& field) {
    uint8_t memory[2] = {static_cast<uint8_t>(field >> 8), static_cast<uint8_t>(field)};
    file.Read(memory, 2);
    field = static_cast<uint16_t>(memory[0] | (memory[1] << 8));
}

}  // namespace

// Sets what the original sets and leaves the rest as the heap had it (see the class).
ESP_CSprite::ESP_CSprite() {
    mLoaded = 0;
    mFile = nullptr;
    mRecordCount = 0;
    mCustomCount = 0;
    mActionCount = 0;
    mSemitransparent = 1;
    for (int32_t block = 0; block < 4; ++block) {
        mCustom[block * 4 + 0] = 0;
        mCustom[block * 4 + 2] = 0;
        mCustom[block * 4 + 3] = 0;
    }
    for (int32_t i = 0; i < 16; ++i) {
        mActions[i].mFirstFrame = 0;
        mActions[i].mFrames = 0;
        mActions[i].mViews = 0;
    }
}

uint8_t ESP_CSprite::GetActionFrames(int32_t action) const { return mActions[action].mFrames; }

void ESP_CSprite::GetCustomData(int32_t block, int32_t* first, ACTS* second, int32_t* third, int32_t* fourth) const {
    if (block < 0 || block > 3) return;
    const uint8_t* bytes = &mCustom[block * 4];
    *second = static_cast<ACTS>(static_cast<int32_t>(bytes[1]) - 1);
    *first = static_cast<int32_t>(bytes[0]) - 1;
    *third = bytes[2];
    *fourth = bytes[3];
}

bool ESP_CSprite::LoadFromESPFile(CFileInfo& info, CFileStorage& storage) {
    // The name is the entry's buffer (the entry's mName, which can point into it, only goes to the trace).
    TbDynamicStringTemplate<char> path(info.mNameBuffer);
    CFile* file = new CFile;
    if (file == nullptr) return false;   // (the original's test of operator new; never taken)
    CTRACE(6, "Found file %s\n", info.mName);
    const TbError opened = storage.OpenFile(*file, info.mNameBuffer, 0x80000001, nullptr);
    if (!opened.Succeeded()) return false;

    // The header: the signature, the version and the data file's name are read and dropped.
    uint8_t header[0x100];
    file->Read(header, 8);
    file->Read(header, 4);
    file->Read(header, 0x100);
    file->Read(&mHasFirstPerson, 1);
    file->Read(&mF206, 1);

    // The name without its extension (the length of ".tps", all the extensions are four long).
    const int32_t nameLength = path.Length();
    const int32_t extensionLength = gTPGlobals.mF42E0.Length();
    const TbDynamicStringTemplate<char> stem(path.Left(static_cast<uint32_t>(nameLength - extensionLength)));
    std::strcpy(mName, stem.mData);

    // The actions.
    mActionCount = 0;
    for (int32_t i = 0; i < 16; ++i) {
        Action& action = mActions[i];
        ReadLittleEndian16(*file, action.mFirstFrame);
        file->Read(&action.mFrames, 1);
        file->Read(&action.mViews, 1);
        if (action.mFrames != 0) ++mActionCount;
    }

    // The custom blocks, when the file has them: the answer of the read of the first byte decides, and any answer but 0
    // (a failed read gives -1) goes on to read the other fifteen.
    for (int32_t block = 0; block < 4; ++block) mCustom[block * 4] = 0;
    mCustomCount = 0;
    if (file->Read(&mCustom[0], 1) != 0) {
        for (int32_t i = 1; i < 16; ++i) file->Read(&mCustom[i], 1);
    }
    for (int32_t block = 0; block < 4; ++block) {
        if (mCustom[block * 4] != 0) ++mCustomCount;
    }
    file->Close();

    // The data file: the same name with the extension of the sprite kind. The first person kind needs a first person
    // game and a set that has one; gESPCompressed picks the "c" extensions over the "s" ones.
    const int32_t length = path.Length();
    const int32_t shortLength = gTPGlobals.mF42E0.Length();
    path = path.Left(static_cast<uint32_t>(length - shortLength));
    const bool firstPerson = gESPFirstPerson != 0 && mHasFirstPerson != 0;
    if (gESPCompressed != 0) {
        if (firstPerson) path += static_cast<const TbStringBase<char>&>(gTPGlobals.mF431C);      // ".fpc"
        else path += static_cast<const TbStringBase<char>&>(gTPGlobals.mF42F4);                  // ".tpc"
    } else {
        if (firstPerson) path += static_cast<const TbStringBase<char>&>(gTPGlobals.mF4308);      // ".fps"
        else path += static_cast<const TbStringBase<char>&>(gTPGlobals.mF42E0);                  // ".tps"
    }
    const TbError reopened = storage.OpenFile(*file, path.mData, 0x80000001, nullptr);
    if (!reopened.Succeeded()) return false;

    // The file object and the sprite file are never freed here (ESP_RemoveSprites frees the latter). A failed Load still
    // counts: mLoaded stays 0 and the answer is 1.
    mFile = new CSpriteFileIO;
    const int32_t loaded = CSpriteFileIO::Load(mFile, file);   // (the caller also clears r5, which Load never reads)
    mLoaded = loaded == 1 ? 1 : 0;
    mRecordCount = static_cast<uint32_t>(mFile->mCount);
    mSemitransparent = gESPSemitransparent;
    if (mSemitransparent != 0) mFile->FlagAsSemitransparent();
    ++gESPSpriteCount;
    return true;
}

// ---- the file list
void ESP_SortFileList() {
    int32_t swapped = 1;
    while (swapped != 0) {
        swapped = 0;
        for (int32_t i = 1; i < gESPFileCount; ++i) {
            ESPFileEntry& previous = gESPFiles[i - 1];
            ESPFileEntry& current = gESPFiles[i];
            const TbDynamicStringTemplate<char> previousName(previous.mInfo->mName);
            const TbDynamicStringTemplate<char> currentName(current.mInfo->mName);
            if (!TPStrGtr(previousName, currentName)) continue;
            const ESPFileEntry held = current;
            ++swapped;
            current = previous;
            previous = held;
        }
    }
}

namespace {

// Adds the entries FindFirst and FindNext give for `pattern` to gESPFiles, copies of the directory entry and of the
// storage it was found in; once the list is full the search goes on and keeps nothing.
void ListFiles(CTPSpriteResource& resource, const TbDynamicStringTemplate<char>& pattern) {
    if (resource.FindFirst(pattern, gESPFileInfo) != 1) return;
    do {
        CFileStorage* found = resource.pGetCurrentSearchStorage();
        if (gESPFileCount < kESPMaxFiles) {
            ESPFileEntry& entry = gESPFiles[gESPFileCount];
            entry.mInfo = new CFileInfo;
            entry.mStorage = new CFileStorage;
            *entry.mInfo = gESPFileInfo;
            *entry.mStorage = *found;
            ++gESPFileCount;
        }
    } while (resource.FindNext(gESPFileInfo) != 0);
}

// Frees the list: the entries and the copies of the storages (the entry pointers are cleared, the storage pointers are not).
void FreeFileList() {
    for (int32_t i = 0; i < gESPFileCount; ++i) {
        ESPFileEntry& entry = gESPFiles[i];
        if (entry.mInfo != nullptr) delete entry.mInfo;
        if (entry.mStorage != nullptr) delete entry.mStorage;
        entry.mInfo = nullptr;
    }
}

// Makes a sprite for the entry at the next free place of gESPSprites and loads it; a sprite that does not load is deleted
// (its place in the table keeps the pointer).
bool LoadFile(ESPFileEntry& entry) {
    ESP_CSprite* sprite = new ESP_CSprite;
    gESPSprites[gESPSpriteCount] = sprite;
    if (!sprite->LoadFromESPFile(*entry.mInfo, *entry.mStorage)) {
        if (gESPSprites[gESPSpriteCount] != nullptr) delete gESPSprites[gESPSpriteCount];
        return false;
    }
    return true;
}

}  // namespace

int32_t ESP_FindESPFiles(CTPSpriteResource& resource, const TbDynamicStringTemplate<char>& name) {
    TbDynamicStringTemplate<char> pattern;
    gESPSemitransparent = 1;
    int32_t made = 0;
    int32_t kidLimit = 0;
    int32_t staffLimit = 0;
    bool kids = false;
    bool staff = false;

    // Kids stop at the player's number of kid sprites, staff at the number of staff sprites.
    if (TPStrEq(name, TbDynamicStringTemplate<char>("kids")) || TPStrEq(name, TbDynamicStringTemplate<char>("kidsheads"))) {
        kids = true;
        kidLimit = gOptions.GetNumKidSprites();
    }
    if (TPStrEq(name, TbDynamicStringTemplate<char>("handymen")) ||
        TPStrEq(name, TbDynamicStringTemplate<char>("mechanics")) ||
        TPStrEq(name, TbDynamicStringTemplate<char>("guards")) ||
        TPStrEq(name, TbDynamicStringTemplate<char>("researchers"))) {
        staff = true;
        staffLimit = gOptions.GetNumStaffSprites();
    }

    gESPFileCount = 0;
    for (int32_t i = 0; i < kESPMaxFiles; ++i) {
        gESPFiles[i].mInfo = nullptr;
        gESPFiles[i].mStorage = nullptr;
        gESPFiles[i].mLoaded = 0;
    }

    // The generic folder, then the current theme's.
    std::strcpy(gESPPathBuffer, "generic:");
    std::strcat(gESPPathBuffer, name.mData);
    std::strcat(gESPPathBuffer, ":");
    pattern = "generic";
    FS_AddDirectorySeparator(pattern);
    pattern += static_cast<const TbStringBase<char>&>(name);
    FS_AddDirectorySeparator(pattern);
    pattern += static_cast<const TbStringBase<char>&>(gTPGlobals.mF4588);
    ListFiles(resource, pattern);

    std::strcpy(gESPPathBuffer, gGame.GetCurrentThemeName().mData);
    std::strcat(gESPPathBuffer, ":");
    std::strcat(gESPPathBuffer, name.mData);
    std::strcat(gESPPathBuffer, ":");
    pattern = gGame.GetCurrentThemeName();
    FS_AddDirectorySeparator(pattern);
    pattern += static_cast<const TbStringBase<char>&>(name);
    FS_AddDirectorySeparator(pattern);
    pattern += static_cast<const TbStringBase<char>&>(gTPGlobals.mF4588);
    ListFiles(resource, pattern);

    ESP_SortFileList();

    // The kids' four preferred files go first, in that order, so that a small number of kid sprites gets those.
    if (kids) {
        CTRACE(6, "Loading Avatar Sprites from %s\n", name.mData);
        for (int32_t preferred = 0; preferred < 4; ++preferred) {
            for (int32_t i = 0; i < gESPFileCount; ++i) {
                ESPFileEntry& entry = gESPFiles[i];
                bool wanted;
                {
                    const TbDynamicStringTemplate<char> fileName(entry.mInfo->mName);
                    const TbDynamicStringTemplate<char> preferredName(gESPKidFiles[preferred]);
                    wanted = TPStrEq(fileName, preferredName);
                }
                if (!wanted || entry.mLoaded != 0) continue;
                if (!LoadFile(entry)) continue;
                ++made;
                entry.mLoaded = 1;
                if (gESPSpriteCount > kESPMaxSprites - 1 || made >= kidLimit) {
                    FreeFileList();
                    return made;
                }
            }
        }
    }

    // Then every file not loaded yet, in name order.
    CTRACE(6, "Loading Sprites from %s\n", name.mData);
    for (int32_t i = 0; i < gESPFileCount; ++i) {
        ESPFileEntry& entry = gESPFiles[i];
        if (entry.mLoaded != 0) continue;
        if (!LoadFile(entry)) continue;
        ++made;
        entry.mLoaded = 1;
        if (gESPSpriteCount > kESPMaxSprites - 1 || (kids && made >= kidLimit) || (staff && made >= staffLimit)) {
            FreeFileList();
            return made;
        }
    }
    FreeFileList();
    return made;
}

bool ESP_LoadSprites(bool particlesOnly) {
    CTPBaseResource* found = nullptr;
    gESPSpriteCount = 0;
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4664, &found) != 1) return false;
    CTPSpriteResource& resource = static_cast<CTPSpriteResource&>(*found);
    if (particlesOnly) {
        ESPSpriteGroup& group = gESPGroups[kESPGroups - 1];
        group.mFirst = gESPSpriteCount;
        const TbDynamicStringTemplate<char> folder(group.mName);
        group.mCount = ESP_FindESPFiles(resource, folder);
    } else {
        for (int32_t i = 0; i < kESPGroups; ++i) {
            ESPSpriteGroup& group = gESPGroups[i];
            group.mFirst = gESPSpriteCount;
            const TbDynamicStringTemplate<char> folder(group.mName);
            group.mCount = ESP_FindESPFiles(resource, folder);
        }
    }
    gESPTotalSprites = gESPSpriteCount;
    return true;
}

// ---- ESP_InitialiseAndLoadSprites (HLSprite.h declares it)
// The sprite lists: sixteen, one for each sixteenth of the map (four by four, the last row and column taking what the
// division leaves), each linked into the map's cells; and one more, linked as map list 0x20. The original answers 1;
// nothing reads the answer.
void ESP_InitialiseAndLoadSprites(bool firstPerson, bool particlesOnly) {
    const sHField* field = gHeightfieldHeader->mHField;
    gESPDrawEnabled = 1;
    gESPF20 = nullptr;
    gESPF88 = 0.0f;
    gei_Display.mFlags |= 0x1000000;
    gSpriteHotX = 0.0f;
    gSpriteHotY = 0.0f;
    gESPFirstPerson = firstPerson ? 1 : 0;
    gESPThirdPerson = firstPerson ? 0 : 1;
    ESP_LoadSprites(particlesOnly);
    gESPF34 = 0;
    gESPMapWidth = static_cast<int32_t>(field->mF18[0]);
    gESPMapHeight = static_cast<int32_t>(field->mF18[1]);
    gESPCellsWide = gESPMapWidth / 4 + 1;
    gESPCellsHigh = gESPMapHeight / 4 + 1;
    for (int32_t row = 0; row < 4; ++row) {
        for (int32_t column = 0; column < 4; ++column) {
            CSpriteList& list = gHLSpriteLists[row * 4 + column];
            gESPListInstances[row][column] = list.Init(0x800);
            list.mCount = 0;
            const int32_t x0 = row * gESPCellsWide;
            const int32_t y0 = column * gESPCellsHigh;
            int32_t x1 = x0 + gESPCellsWide - 1;
            int32_t y1 = y0 + gESPCellsHigh - 1;
            if (row == 3) x1 = static_cast<int32_t>(field->mF18[0]) - 1;
            if (column == 3) y1 = static_cast<int32_t>(field->mF18[1]) - 1;
            gei_MapWho->Link(x0, y0, x1, y1, static_cast<CMapLinkBase*>(&list), 0);
        }
    }
    gESPGlobalInstances = gESPGlobalList.Init(0x20);
    gESPGlobalList.mCount = 0;
    gei_MapWho->Link(static_cast<CMapLinkBase*>(&gESPGlobalList), 0x20);
}

// HLSprite.cpp (main app 0x1007c790): the end of the people sprites. ESP_InitialiseAndLoadSprites (src/ESP_sprites.cpp) made them.

// Gives back every sprite set the loader made: the sprite file's data (CSpriteFileIO::Free) and the file object, for the sets
// that loaded, then the set itself; then the sixteen sprite lists (4 x 4) and the global list are closed. gESPSpriteCount and the
// pointer array are left as they are. The sets are not tested for NULL, and the first Free runs on the file pointer whatever it
// holds (the Mac tests only the second); the file pointer is read again after the Free, as the Mac does.
void ESP_RemoveSprites() {
    for (int32_t i = 0; i < gESPSpriteCount; ++i) {
        ESP_CSprite* sprite = gESPSprites[i];
        if (sprite->mLoaded != 0) {
            sprite->mFile->Free();
            CSpriteFileIO* file = sprite->mFile;
            if (file != nullptr) {
                file->Free();
                delete file;
            }
            sprite->mFile = nullptr;
        }
        ESP_CSprite* again = gESPSprites[i];
        if (again != nullptr) delete again;
    }
    for (int32_t row = 0; row < 4; ++row)
        for (int32_t column = 0; column < 4; ++column) gHLSpriteLists[row * 4 + column].Close();
    gESPGlobalList.Close();
}

// 0x1007c8b0
int32_t ESP_GetNumberOfSprites(ESP_Sprites set) { return gESPGroups[set].mCount; }

// Unit v51049: std::locale::~locale 0x10070d44, std::basic_filebuf<char>::imbue 0x10070bf4 and pbackfail 0x100715b4,
// DisableRideHighlight 0x100672e4, ESP_GetRandomSpritesFrom 0x1007c900, ToggleProceduralTexturing 0x100634f8,
// DestroyDynamicMeshSetInstance 0x10063264, CAdvisor::SetPlayingFor 0x1000c7fc.

namespace {
// The 32-bit words of the Mac's objects are arena offsets here (include/Ptr32.h): 0 is NULL.
inline uint32_t W(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }
inline void* P(uint32_t w) { return w != 0 ? tpw::gArenaBase + w : nullptr; }
}  // namespace

// ---- std::locale::~locale (0x10070d44): the last reference destroys the impl and the count; flag > 0 (signed 16 bit) deletes the locale.
StdLocaleWords* LocaleStdDestroy(StdLocaleWords* self, int16_t flag) {
    if (self) {
        uint32_t count = self->mCount;
        if (count) {
            int32_t* c = static_cast<int32_t*>(P(count));
            int32_t v = *c - 1;
            *c = v;
            if (v == 0) {
                filebuf_wclose_callee::LocaleImpDtor(P(self->mImp), 1);
                locale_imp_callee::Delete(P(self->mCount));
            }
        }
        if (flag > 0) locale_imp_callee::Delete(self);
    }
    return self;
}

// ---- std::basic_filebuf<char>::imbue (0x10070bf4)
void BasicFilebufChar::imbue(const void* loc) {
    streambuf_callee::LocaleAssign(&mLocale, loc);
    uint32_t tmp[2];
    filebuf_char_callee::Getloc(tmp, this);
    void* facet = filebuf_char_callee::UseFacetCodecvt(tmp);
    mCodecvt = W(facet);
    uint32_t count = tmp[1];
    if (count) {
        int32_t* c = static_cast<int32_t*>(P(count));
        int32_t v = *c - 1;
        *c = v;
        if (v == 0) {
            filebuf_wclose_callee::LocaleImpDtor(P(tmp[0]), 1);
            locale_imp_callee::Delete(P(tmp[1]));
        }
    }
    mUnbuffered = static_cast<uint8_t>(filebuf_char_callee::CodecvtAlwaysNoconv(P(mCodecvt)));
}

// ---- std::basic_filebuf<char>::pbackfail (0x100715b4): step the get pointer back (unsigned compare with the start).
int32_t BasicFilebufChar::pbackfail(int32_t c) {
    if (mFile == 0 || !(mGptr > mEback)) return -1;
    mGptr = mGptr - 1;
    if (c != -1) {
        *static_cast<char*>(P(mGptr)) = static_cast<char>(c);
    }
    if (c == -1) c = 0;
    return c;
}

// ---- ESP_GetRandomSpritesFrom (0x1007c900): a random index below the group's count (the count is never 0 in the game;
// the divide would trap here).
int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    uint32_t count = static_cast<uint32_t>(gESPGroups[static_cast<int32_t>(set)].mCount);
    uint32_t r = gGameWorld->GetRand() >> 2;
    return static_cast<int32_t>(r % count);
}

// 0x1007c990
int32_t ESP_GetNumberOfCustomAnims(ESP_Sprites set, int32_t index) {
    return gESPSprites[gESPGroups[set].mFirst + index]->mCustomCount;
}

// Unit v51046: nine small functions of the lobby, the interface and the ESP sprites (main app).
//   OnlineIslandParkDownloadCompleteCallback 0x100b4420, PostGetTop10ParksListCallbackFunction 0x100b3e28,
//   CLBScreenComponentIslands::ProcessCancel 0x100b1f3c, __sinit_lobby_parser_cpp 0x100b6ae4,
//   CLBScreenComponentDownloadGlobe::Show 0x100ae590, CLBScreenComponentDownloadGlobe::ProcessCancel 0x100ae46c,
//   CIntrface::DrawFPSCounter 0x1007dae8, __sinit_HLSprite_cpp 0x1007d53c, ESP_GetRandomActionFrom 0x1007c9f8.

extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count);   // runtime glue 0x10238758
// ---- ESP sprites

int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites) {
    const ESP_CSprite* sprite = gESPSprites[sprites + gESPGroups[set].mFirst];
    uint32_t count = sprite->mActionCount;
    uint32_t number = gGameWorld->GetRand() >> 2;
    uint32_t quotient = count != 0 ? number / count : 0;   // divwu by 0 answers 0 in the emulator
    return static_cast<int32_t>(number - quotient * count);
}

// ESP_sprites_ESP_SwitchToFirstPersonSprites.cpp: the rest of HLSprite.cpp's sprite code (unit v5027):
//   0x1007cfc8 ESP_SwitchToFirstPersonSprites, 0x1007d280 ESP_SwitchToThirdPersonSprites (reload every sprite set's data
//   file with the other view's extension) and 0x1007ca9c ESP_AddHLSpriteFromScript (fills the sprite record embedded in a
//   script and links it into a cell's list).

// Globals that only this file uses (initialised data on the Mac; names invented).
int32_t gESPMaxCellX = 3;                // 0x1029d420
int32_t gESPMinCellX = 0;                // 0x1029d424
int32_t gESPMaxCellZ = 3;                // 0x1029d428
int32_t gESPMinCellZ = 0;                // 0x1029d42c
int32_t gESPGlobalCount = 0;             // 0x102dbd40
int32_t gESPCellCounts[4][4];            // 0x102dbd7c

// The adder copies the record's first ten words to the host record as they are.
static_assert(offsetof(SSpriteGameData, mF24) == 0x24 && offsetof(SSpriteGameData, mSprite) == 0x28,
              "SSpriteGameData's first ten words are the Mac's");

namespace {

// fctiwz: truncation towards zero, saturating; NaN gives 0x80000000.
int32_t Truncate(float value) {
    if (value != value) return INT32_MIN;
    if (value >= 2147483648.0f) return INT32_MAX;
    if (value <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(value);
}

// Both switches: reload the data file of every sprite set that has first person sprites, with the extension of the
// wanted view. `third` is the new value of gESPThirdPerson; the switch does nothing when it already holds it.
int32_t SwitchSprites(int32_t third) {
    TbDynamicStringTemplate<char> path;
    TbDynamicStringTemplate<char> extension;
    CTPBaseResource* resource = nullptr;
    if (gESPThirdPerson == third) return 0;
    gESPThirdPerson = third;
    if (third == 0) {
        if (gESPCompressed != 0) extension = gTPGlobals.mF431C;
        else extension = gTPGlobals.mF4308;
    } else {
        if (gESPCompressed != 0) extension = gTPGlobals.mF42F4;
        else extension = gTPGlobals.mF42E0;
    }
    if (gGame.mResourceManager->FindResource(gTPGlobals.mF4664, &resource) != 1) return 0;
    for (int32_t i = 0; i < gESPTotalSprites; i++) {
        ESP_CSprite* sprite = gESPSprites[i];
        if (sprite->mHasFirstPerson == 0) continue;
        path = sprite->mName;
        path += extension;
        sprite->mFile->Free();     // no NULL check in the original
        CSpriteFileIO* old = sprite->mFile;
        if (old != nullptr) {
            old->Free();
            delete old;
        }
        sprite->mFile = nullptr;
        bfl::FileSystem::CFile* file = new bfl::FileSystem::CFile();
        if (resource->OpenQualifiedFile(*file, path) == 1) {
            sprite = gESPSprites[i];
            CSpriteFileIO* io = new CSpriteFileIO();
            sprite->mFile = io;
            int32_t loaded = CSpriteFileIO::Load(sprite->mFile, file);
            sprite->mLoaded = loaded == 1 ? 1 : 0;
            sprite->mRecordCount = static_cast<uint32_t>(sprite->mFile->mCount);
            sprite = gESPSprites[i];
            if (sprite->mSemitransparent != 0) sprite->mFile->FlagAsSemitransparent();
        }
    }
    return 1;
}

}  // namespace

int32_t ESP_SwitchSprites(int32_t third) { return SwitchSprites(third); }

void ESP_AddHLSpriteFromScript(tagSCRIPT* script) {
    uint8_t* b = reinterpret_cast<uint8_t*>(script);
    auto i32 = [b](uint32_t o) -> int32_t& { return *reinterpret_cast<int32_t*>(b + o); };
    auto u32 = [b](uint32_t o) -> uint32_t& { return *reinterpret_cast<uint32_t*>(b + o); };
    auto f32 = [b](uint32_t o) -> float& { return *reinterpret_cast<float*>(b + o); };
    // The embedded sprite record holds 32-bit pointers on the Mac; the arena the host's blocks come from is 32-bit.
    auto put = [b](uint32_t o, const void* p) {
        uint32_t v = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
        std::memcpy(b + o, &v, 4);
    };

    if (script->mHasSprite == 0) return;                // +0x114
    const int32_t frameBase = i32(0xb8);
    if (frameBase == -1) return;
    if (gESPDrawEnabled == 0) return;

    const int32_t kind = i32(0xac);
    const uint32_t flags = u32(0xc4);
    bool mirrored = false;
    uint32_t outFlags = 0;
    const uint32_t direction = u32(0xa0) & 0xff;
    const int32_t viewOffset = i32(0xc0);
    const float f31 = f32(0xa4);
    const float f30 = f32(0xa8);
    int32_t setIndex = i32(0xb0) + gESPGroups[kind].mFirst;
    const int32_t action = i32(0xb4);
    const float x = f32(0x88);
    float y = f32(0x8c);
    const float z = f32(0x90);
    if ((flags & 0x1000000) != 0) {
        f32(0xe4) = x;
        f32(0xe8) = f32(0x8c);
        f32(0xec) = z;
    } else {
        float height = DetermineHFieldHeightQuick(x, z);
        y = y + height;
    }
    const int32_t ix = Truncate(x);
    const int32_t iz = Truncate(z);
    int32_t cellX = (ix / 10) / gESPCellsWide;
    int32_t cellZ = (iz / 10) / gESPCellsHigh;
    cellX = (gESPMaxCellX >= cellX) ? cellX : gESPMaxCellX;
    cellX = (cellX >= gESPMinCellX) ? cellX : gESPMinCellX;
    cellZ = (gESPMaxCellZ >= cellZ) ? cellZ : gESPMaxCellZ;
    cellZ = (cellZ >= gESPMinCellZ) ? cellZ : gESPMinCellZ;
    if (gESPCellCounts[cellX][cellZ] >= 0x800) return;

    const int32_t set = action / 16;
    setIndex += set;
    const int32_t actionNumber = action - set * 16;
    ESP_CSprite* sprite = gESPSprites[setIndex];
    const ESP_CSprite::Action& act = sprite->mActions[actionNumber];
    const uint32_t first = act.mFirstFrame;
    if ((flags & 0x40000) != 0) {
        gESPF34 = 0;
    } else {
        float t = 0.39269909262657166f + gESPF88;
        float q = t / 6.2831854820251465f;
        float scaled = 8.0f * q;
        gESPF34 = 8 - (Truncate(scaled) & 7);
    }
    const int32_t views = act.mViews;
    int32_t view;
    if (views == 0 || (flags & 0x2000000) != 0) {
        view = 0;
    } else {
        view = (viewOffset + gESPF34) & 7;
        if (view > views - 1) {
            mirrored = true;
            view = 8 - view;
        }
    }
    const int32_t frames = act.mFrames;
    int32_t frame = frameBase + view * frames;
    if (kind == 5) outFlags = 0x100000;
    if (mirrored) outFlags |= 0x10;
    u32(0xe0) = (flags & 0xffffff) | outFlags;
    frame = static_cast<int32_t>(first) + frame;
    f32(0xe4) = x;
    f32(0xe8) = y;
    f32(0xec) = z;
    f32(0xfc) = f31;
    f32(0x100) = f30;
    u32(0xf4) = direction | 0xffffff00u;
    CSpriteFileIO* file = gESPSprites[setIndex]->mFile;
    const SSpriteEngineData* record;
    if (frame < file->mCount && frame >= 0) record = file->mRecords + frame;
    else record = file->mRecords;
    put(0xf8, record);
    if ((flags & 0x200) != 0) {
        int32_t v = viewOffset;
        if (viewOffset > views - 1) v = 8 - viewOffset;
        put(0x104, gESPSprites[setIndex]->mFile);
        b[0x10a] = static_cast<uint8_t>(views * 16 + frames);
        b[0x10b] = static_cast<uint8_t>(v * 16 + frameBase);
        const uint16_t half16 = static_cast<uint16_t>(first);
        std::memcpy(b + 0x108, &half16, 2);
    }
    // HOST: the record at b + 0xd0 keeps the Mac's layout (32-bit pointers); the sprite engine reads the host's
    // SSpriteGameData, so the script's host record (src/ScriptHostSprites.cpp) takes the record's fields and is listed.
    SSpriteGameData* listed = ESP_ScriptHostSprite(script);
    std::memcpy(listed, b + 0xd0, offsetof(SSpriteGameData, mSprite));   // 0x00..0x27: the same words on both
    listed->mSprite = const_cast<SSpriteEngineData*>(record);
    std::memcpy(&listed->mF2C, b + 0xfc, 4);
    std::memcpy(&listed->mRotation, b + 0x100, 4);
    if ((flags & 0x200) != 0) {
        listed->mFrameFile = gESPSprites[setIndex]->mFile;
        listed->mFrameBase = static_cast<uint16_t>(first);
    }
    std::memcpy(listed->mLightFrames, b + 0x10c, 4);
    if ((flags & 0x40000) != 0) {
        gESPGlobalInstances[gESPGlobalCount] = listed;
        gESPGlobalCount = gESPGlobalCount + 1;
    } else {
        gESPListInstances[cellX][cellZ][gESPCellCounts[cellX][cellZ]] = listed;
        gESPCellCounts[cellX][cellZ] = gESPCellCounts[cellX][cellZ] + 1;
    }
}

// The particle frame members read the answer as a PTCLTexture (include/tb_particle.h): its words are the sprite's first three.
static_assert(offsetof(PTCLTexture, mEntry) == offsetof(SSpriteEngineData, mTexture) &&
                  offsetof(PTCLTexture, mF04) == offsetof(SSpriteEngineData, mData) &&
                  offsetof(PTCLTexture, mAspect) == offsetof(SSpriteEngineData, mAspect),
              "a particle texture is the start of a sprite of the particle sprite file");

PTCLTexture* ESP_GetParticleSprite(int32_t sprite, int32_t frame, int32_t variant) {
    // Division and remainder round towards zero, as the compiled shift and adjust do.
    const int32_t set = sprite / 16;
    const int32_t actionNumber = sprite - set * 16;
    const ESP_CSprite* particles = gESPSprites[gESPGroups[kESPGroups - 1].mFirst + set];
    const ESP_CSprite::Action& action = particles->mActions[actionNumber];
    const int32_t view = action.mViews == 0 ? 0 : (variant & 7);
    const int32_t index = static_cast<int32_t>(action.mFirstFrame) + (frame + view * static_cast<int32_t>(action.mFrames));
    const CSpriteFileIO* records = particles->mFile;
    if (index < records->mCount && index >= 0) return reinterpret_cast<PTCLTexture*>(records->mRecords + index);
    return reinterpret_cast<PTCLTexture*>(records->mRecords);
}

void ESP_SwitchToFirstPersonSprites() { SwitchSprites(0); }

void ESP_SwitchToThirdPersonSprites() { SwitchSprites(1); }

// Stand-ins for the Mac's words (the imports' addresses are not callable here).
void HLSpriteListCtorGlue(void*) {}
void HLSpriteListDtorGlue(void*) {}
uint32_t gHLSpriteArrayDtor[2];
uint32_t gESPFileInfoDtor[2];
uint8_t gHLSpriteArrayNode[12];
uint8_t gESPGlobalListNode[12];
uint8_t gESPFileInfoNode[12];

void __sinit_HLSprite_cpp() {
    __construct_array(gHLSpriteLists, reinterpret_cast<void*>(&HLSpriteListCtorGlue),
                      reinterpret_cast<void*>(&HLSpriteListDtorGlue), 0x28, 0x10);
    __register_global_object(nullptr, gHLSpriteArrayDtor, gHLSpriteArrayNode);
    CSpriteList* globalList = ::new (&gESPGlobalList) CSpriteList();
    __register_global_object(globalList, reinterpret_cast<void*>(&HLSpriteListDtorGlue), gESPGlobalListNode);
    ::new (&gESPFileInfo) bfl::FileSystem::CFileInfo();   // the name pointer at +0x20 to the buffer at +0x24, which starts empty
    __register_global_object(&gESPFileInfo, gESPFileInfoDtor, gESPFileInfoNode);
}

namespace {

// USwapInplace32 (lwz, stwbrx): the word where it is, byte-reversed. The long, unsigned long and float instances all do this.
inline void SwapWordAt(uint8_t* p) {
    uint8_t b[4];
    std::memcpy(b, p, 4);
    p[0] = b[3];
    p[1] = b[2];
    p[2] = b[1];
    p[3] = b[0];
}

}  // namespace

namespace SamsUtilities {

// 0x10079740 (480 bytes): swaps every word of `count` records of 0xe4 bytes: words at 0x00..0x30, floats 0x34..0x48,
// words 0x4c..0x8c, the nine words at 0x90 (USwapBlock32 x 9), then words 0xb4..0xe0. Every field is a 32-bit word.
void USwapBlockStruct(FLYBOY* records, int32_t count) {
    uint8_t* record = reinterpret_cast<uint8_t*>(records);
    while (count-- != 0) {
        for (int32_t offset = 0; offset < 0xe4; offset += 4) SwapWordAt(record + offset);
        record += 0xe4;
    }
}

}  // namespace SamsUtilities

// __arraydtor$3071 at 0x1007d614 destroys this array (40-byte original elements).
// __sinit_HLSprite_cpp at 0x1007d53c constructs it and registers that helper.
// Native C++ supplies array construction, reverse destruction and registration.
CSpriteList gHLSpriteLists[16];

// Unit v51119: the world globe's camera steps and list registration (CLBScreenComponentOnlineGlobe), and the small
// destructors and byte swaps the neighbouring addresses hold.

// 0x100700e4
BasicFilebufChar* BasicFilebufChar_DtorMac(BasicFilebufChar* self, int16_t flag) {
    if (self != nullptr) {
        self->mVtable = A(gFilebufCharVtblB);
        uint32_t files = A(gIfsWFiles);
        if (self->mFile != files && self->mFile != files + 0x54 && self->mFile != files + 0xa8) OfstreamClose(self);
        self->mVtable = A(gFilebufCharVtblA);
        if (A(self) + 0x1c != 0) OfstreamLocaleDtor(&self->mLocale, -1);
        if (flag > 0) OstreamDelete(self);
    }
    return self;
}

// 0x1006ff50
void* BasicIosChar_DtorMac(void* self, int16_t flag) {
    if (self != nullptr) {
        *static_cast<uint32_t*>(self) = A(gOstreamVt1);
        OstreamIosBaseDtor(static_cast<MacOstreamIos*>(self), 0);
        if (flag > 0) OstreamDelete(self);
    }
    return self;
}

// ---- SamsUtilities byte swaps: the value's bytes reversed in place
namespace v51119_swap {
void USwapInplace16_short_100de404(int16_t& v) { v = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(v))); }
void USwapInplace32_long_100de448(int32_t& v) { v = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(v))); }
void USwapInplace32_long_10079a64(int32_t& v) { v = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(v))); }
void USwapInplace32_ulong_10079968(uint32_t& v) { v = __builtin_bswap32(v); }
void USwapInplace32_ulong_1006b6ac(uint32_t& v) { v = __builtin_bswap32(v); }
void USwapInplace32_int_1006b668(int32_t& v) { v = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(v))); }
void USwapInplace16_ushort_1006b620(uint16_t& v) { v = __builtin_bswap16(v); }
// 0x100799b0: hands both arguments to _USwapBlock32
void USwapBlock32_long_100799b0(int32_t* words, int32_t count) {
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(words), count);
}
}  // namespace v51119_swap
