// Test library for the sprite switches and ESP_AddHLSpriteFromScript (unit v5027; libtpw_unit_ESP_SwitchToFirstPersonSprites.dylib,
// tests/test_ESP_SwitchToFirstPersonSprites.py), main app:
//   src/game/HLSprite.cpp   the three functions and the globals only they use
//
// The callees outside the unit (string objects, the resource manager, the file objects, CSpriteFileIO's Free/Load/Flag,
// operator delete, DetermineHFieldHeightQuick) are fakes that record what the PowerPC stubs record. The globals of other files
// are defined here; gTPGlobals and gGame are raw bytes under the program's symbol names (their constructors are not linked).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include <cstdarg>

#include "calllog.h"
#include "platform/arena.h"
#include "TbDynamicStringTemplate.h"

using namespace calllog;

// ---- the string object's fakes (before any other header instantiates the class). Every virtual that is not inline is defined,
// so that the vtable the constructor sets exists; the translation only calls them on locals, directly.
template <> TbDynamicStringTemplate<char>::TbDynamicStringTemplate() { ev("StrCtor", {newlab(this)}); }
template <> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() { ev("StrDtor", {lab(this)}); }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>& other) {
    ev("AssignStr", {lab(this), lab(&other)});
    return *this;
}
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char* text) {
    ev("AssignC", {lab(this)}, {text});
    return *this;
}
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>& other) {
    ev("Append", {lab(this), lab(&other)});
    return *this;
}
template <> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template <> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

#include "ESP_sprites.h"
#include "Game.h"
#include "HLSprite.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TbDynamicStringTemplate.h"
#include "analyser.h"
#include "sprite_engine.h"

// ---- globals of other files, defined here
uint8_t gESPCompressed;
int32_t gESPThirdPerson;
uint8_t gESPDrawEnabled;
ESPSpriteGroup gESPGroups[kESPGroups];
int32_t gESPTotalSprites;
ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
int32_t gESPCellsHigh;
int32_t gESPCellsWide;
int32_t gESPF34;
float gESPF88;
SSpriteGameData** gESPListInstances[4][4];
SSpriteGameData** gESPGlobalInstances;

alignas(16) char gTPGlobalsBytes[sizeof(TPGlobals)] __asm__("_gTPGlobals");
alignas(16) char gGameBytes[sizeof(CGame)] __asm__("_gGame");
static char gResToken[16];
static char gRmToken[16];

namespace {

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// The heap the unit's code deletes from: zeroed blocks, a delete that is logged.

// ---- the other fakes
bfl::FileSystem::CFile::CFile() { ev("FileCtor", {newlab(this)}); }
// The out-of-line virtuals, so that the vtable the constructor sets exists (never called).
bfl::FileSystem::CFile::~CFile() {}
void bfl::FileSystem::CFile::Close() {}
int32_t bfl::FileSystem::CFile::Seek(uint32_t, uint32_t) { return -1; }
int32_t bfl::FileSystem::CFile::GetSize() { return 0; }
int32_t bfl::FileSystem::CFile::GetPos() { return 0; }
void* bfl::FileSystem::CFile::MapFile() { return nullptr; }
TbError bfl::FileSystem::CFile::Flush() { return TbError(0); }
int32_t bfl::FileSystem::CFile::GetMacFileReference() { return 0; }

int32_t CTPResourceManager::FindResource(const TbDynamicStringTemplate<char>& name, CTPBaseResource** resource) {
    ev("Find", {lab(this), lab(&name)});
    *resource = reinterpret_cast<CTPBaseResource*>(gResToken);
    return static_cast<int32_t>(ret("Find"));
}

int32_t CTPBaseResource::OpenQualifiedFile(bfl::FileSystem::CFile& file, const TbDynamicStringTemplate<char>& name) {
    ev("Open", {lab(this), lab(&file), lab(&name)});
    return static_cast<int32_t>(ret("Open"));
}

int32_t CSpriteFileIO::Free() {
    ev("Free", {lab(this)});
    return 1;
}

int32_t CSpriteFileIO::Load(CSpriteFileIO* io, void* file) {
    io->mCount = static_cast<int32_t>(ret("LoadCount"));
    ev("Load", {lab(io), lab(file)});
    return static_cast<int32_t>(ret("LoadRet"));
}

void CSpriteFileIO::FlagAsSemitransparent() { ev("Flag", {lab(this)}); }

float DetermineHFieldHeightQuick(float x, float z) {
    ev("Height", {CanonBits(x), CanonBits(z)});
    uint32_t bits = static_cast<uint32_t>(ret("Height"));
    float out;
    std::memcpy(&out, &bits, 4);
    return out;
}

extern "C" {

uint64_t tpw_esp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    TPGlobals* g = reinterpret_cast<TPGlobals*>(gTPGlobalsBytes);
    CGame* game = reinterpret_cast<CGame*>(gGameBytes);
    const Entry entries[] = {
        {"compressed", &gESPCompressed},
        {"third", &gESPThirdPerson},
        {"draw", &gESPDrawEnabled},
        {"groups", gESPGroups},
        {"total", &gESPTotalSprites},
        {"sprites", gESPSprites},
        {"cellsW", &gESPCellsWide},
        {"cellsH", &gESPCellsHigh},
        {"f34", &gESPF34},
        {"f88", &gESPF88},
        {"counts", gESPCellCounts},
        {"gcount", &gESPGlobalCount},
        {"glist", &gESPGlobalInstances},
        {"lists", gESPListInstances},
        {"maxx", &gESPMaxCellX},
        {"minx", &gESPMinCellX},
        {"maxz", &gESPMaxCellZ},
        {"minz", &gESPMinCellZ},
        {"tpg", gTPGlobalsBytes},
        {"gamerm", &game->mResourceManager},
        {"s4308", &g->mF4308},
        {"s431c", &g->mF431C},
        {"s42e0", &g->mF42E0},
        {"s42f4", &g->mF42F4},
        {"s4664", &g->mF4664},
        {"restoken", gResToken},
        {"rmtoken", gRmToken},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_esp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// A block on the program's heap (the code deletes the old file objects).
uint64_t tpw_esp_heap(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(std::calloc(1, size))); }

int32_t tpw_esp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ESP_CSprite, mLoaded));
    out[n++] = static_cast<int64_t>(offsetof(ESP_CSprite, mRecordCount));
    out[n++] = static_cast<int64_t>(offsetof(ESP_CSprite, mActions));
    out[n++] = static_cast<int64_t>(sizeof(SSpriteEngineData));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mHasSprite));
    out[n++] = static_cast<int64_t>(offsetof(CSpriteFileIO, mRecords));
    out[n++] = static_cast<int64_t>(sizeof(ESPSpriteGroup));
    out[n++] = static_cast<int64_t>(offsetof(ESPSpriteGroup, mFirst));
    out[n++] = static_cast<int64_t>(sizeof(ESP_CSprite));
    return n;
}

uint32_t tpw_esp_first() { return static_cast<uint32_t>(ESP_SwitchSprites(0)); }
uint32_t tpw_esp_third() { return static_cast<uint32_t>(ESP_SwitchSprites(1)); }
void tpw_esp_add(uint64_t script) { ESP_AddHLSpriteFromScript(reinterpret_cast<tagSCRIPT*>(static_cast<uintptr_t>(script))); }

// The script's host record (the sprite the adder lists) and its fields, in the order the test reads the Mac's record:
// the ten words 0x00..0x24, mSprite, mF2C, the rotation word, mFrameFile, mFrameBase, the four light frame bytes (first lowest).
uint64_t tpw_esp_hostsprite(uint64_t script) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ESP_ScriptHostSprite(reinterpret_cast<tagSCRIPT*>(static_cast<uintptr_t>(script)))));
}
void tpw_esp_sprite_fields(uint64_t address, uint64_t* out) {
    const SSpriteGameData* d = reinterpret_cast<const SSpriteGameData*>(static_cast<uintptr_t>(address));
    uint32_t words[10];
    std::memcpy(words, d, sizeof words);
    for (int32_t i = 0; i < 10; ++i) out[i] = words[i];
    out[10] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(d->mSprite));
    uint32_t w;
    std::memcpy(&w, &d->mF2C, 4);
    out[11] = w;
    std::memcpy(&w, &d->mRotation, 4);
    out[12] = w;
    out[13] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(d->mFrameFile));
    out[14] = d->mFrameBase;
    out[15] = static_cast<uint64_t>(d->mLightFrames[0]) | (static_cast<uint64_t>(d->mLightFrames[1]) << 8) |
              (static_cast<uint64_t>(d->mLightFrames[2]) << 16) | (static_cast<uint64_t>(d->mLightFrames[3]) << 24);
}

}  // extern "C"
