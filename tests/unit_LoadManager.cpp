// Test library for src/tb_loadmanager.cpp (libtpw_unit_LoadManager.dylib; see
// tests/test_LoadManager.py): two CsTexture objects, two texture records, the texture
// load manager's table and the delayed-load list, driven by scripts.
//
// Real: CsTexture, CTextureLoadManager::RemoveTexture and SetLoadFlags,
// DelayedLoad_LoadTextures. Fakes that record: the engine allocator (it records how many
// POINTERS a list holds, which is the same number on both sides), the manager's LoadFile
// and LoadTexture (they hand out arrays of blank texture entries), the texture entry's
// constructor, destructor, Close and Create, the error handler, the assertion.
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void ObserveAllocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"

#include "Wavelet.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_loadmanager.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {
int gQuiet;
// An array of texture entries gets a label for its allocation, and each entry one as it
// is constructed (the PowerPC side labels the same addresses in the same order).
void ObserveAllocation(std::size_t, bool array, void* block) {
    if (array && !gQuiet) newlab(block);
}

int32_t gListCounts[1024];   // by label: how many pointers a list from the allocator holds
void* FakeMalloc(uint32_t size) {
    const uint32_t count = size / sizeof(void*);
    ev("Malloc", {count});
    if (!ret("Malloc", 1)) return nullptr;
    void* block = std::calloc(count != 0 ? count : 1, sizeof(void*));
    const int32_t label = newlab(block);
    if (label >= 0 && label < 1024) gListCounts[label] = static_cast<int32_t>(count);
    return block;
}
void FakeFree(void*& block) {
    ev("Free", {lab(block)});
    forget(block);
    std::free(block);
    block = nullptr;
}

alignas(16) unsigned char gObjectStore[2][sizeof(CsTexture)];
CsTexture* Object(int32_t k) { return reinterpret_cast<CsTexture*>(gObjectStore[k & 1]); }
sTexture* gTextures;        // two records, their infos and names: from the arena
sTextureInfo* gInfos;
char* gNames;               // two blocks of 4 names of 0x14 bytes
char* gFolder;

CTextureEntry* NewArray(int32_t count) { return new CTextureEntry[static_cast<size_t>(count)]; }
}  // namespace

// The array the object makes for itself: through the tracked operator, or not at all.
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    if (on() && !ret("NewArray", 1)) return nullptr;
    return operator new[](size);
}

TbMallocFn gMalloc = FakeMalloc;
TbFreeFn gFree = FakeFree;
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Error", {level, code}); }
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
STextureLoad::STextureLoad() {}
CTextureLoadManager::CTextureLoadManager() {}
CTextureLoadManager gManager;
CTextureLoadManager* gei_TLoadManager = &gManager;

CTextureEntry::CTextureEntry() {
    std::memset(static_cast<void*>(this), 0, sizeof *this);
    newlab(this);
}
CTextureEntry::~CTextureEntry() {
    ev("Entry.dtor", {lab(this)});
    forget(this);
}
int32_t CTextureEntry::Close() {
    ev("Entry.Close", {lab(this)});
    return 1;
}
int32_t CTextureEntry::Create(int32_t size, int32_t flags) {
    ev("Entry.Create", {lab(this), size, flags});
    return static_cast<int32_t>(ret("Create", 1));
}
CTextureEntry* CTextureLoadManager::LoadFile(char* name) {
    ev("LoadFile", {}, {name});
    const int32_t count = static_cast<int32_t>(ret("LoadFile", 1));
    return count != 0 ? NewArray(count) : nullptr;
}
CTextureEntry* CTextureLoadManager::LoadTexture(char* folder, char* name) {
    ev("LoadTexture", {mLoadFlags}, {folder != nullptr ? folder : "(null)", name});
    const int32_t count = static_cast<int32_t>(ret("LoadTexture", 1));
    if (count == 0) return nullptr;
    CTextureEntry* entries = NewArray(count);
    entries[0].mWidth = static_cast<int16_t>(ret("Width", 16));
    entries[0].mHeight = static_cast<int16_t>(ret("Height", 16));
    return entries;
}

extern "C" {
void tpw_lm_reset() {
    ++gQuiet;
    std::memset(gObjectStore, 0xa5, sizeof gObjectStore);
    if (gTextures == nullptr) {
        gTextures = static_cast<sTexture*>(tpw::ArenaAllocZeroed(2 * sizeof(sTexture)));
        gInfos = static_cast<sTextureInfo*>(tpw::ArenaAllocZeroed(2 * sizeof(sTextureInfo)));
        gNames = static_cast<char*>(tpw::ArenaAllocZeroed(2 * 4 * 0x14));
        gFolder = static_cast<char*>(tpw::ArenaAllocZeroed(32));
        std::strcpy(gFolder, "data:textures");
    }
    std::memset(gTextures, 0, 2 * sizeof(sTexture));
    std::memset(gInfos, 0, 2 * sizeof(sTextureInfo));
    std::memset(gNames, 0, 2 * 4 * 0x14);
    gManager.mCount = 0;
    gManager.mDefault = -1;
    gManager.mLoadFlags = 0;
    gDelayedLoadCount = 0;
    std::memset(gDelayedLoads, 0, sizeof gDelayedLoads);
    gei_TextureSettings.mFlags = 0;
    std::memset(gListCounts, 0, sizeof gListCounts);
    --gQuiet;
}

int64_t tpw_lm_op(int32_t op, const int32_t* a) {
    CsTexture* object = Object(a[0]);
    switch (op) {
    case 1: new (object) CsTexture; return 0;
    case 2: return object->Load(NewArray(a[1]));
    case 3: {
        char name[32];
        std::strcpy(name, "data:tex.tga");
        return object->Load(name);
    }
    case 4: return object->Load(&gTextures[a[1] & 1], &gInfos[a[1] & 1]);
    case 5: return object->LoadTextureFromDisk();
    case 6: return object->ReLoad(Object(a[1]));
    case 7: return object->Close();
    case 8: object->~CsTexture(); return 0;
    case 9:
        switch (a[2]) {
        case 0: return object->GetFrameCount();
        case 1: return object->GetsTexture() == nullptr ? 0 : 1 + (object->GetsTexture() - gTextures);
        case 2: return lab(object->GetTextureEntry(a[1]));
        default: return object->GetTextureEntryAddr(a[1]) - object->mEntries;
        }
    case 10: object->SetTextureEntries(a[1]); return 0;
    case 11: object->SetTextureEntry(NewArray(1), a[1]); return 0;
    case 12: {   // texture record a[0]: flags a[1], frames a[2], folder or none a[3], size word a[4]
        const int32_t t = a[0] & 1;
        gTextures[t].mF00 = static_cast<uint16_t>(a[1]);
        gInfos[t].mFrames = static_cast<uint16_t>(a[2]);
        gInfos[t].mFolder = a[3] ? gFolder : nullptr;
        char* names = gNames + t * 4 * 0x14;
        gInfos[t].mName = names;
        for (int32_t i = 0; i < 4; ++i) {
            char* name = names + i * 0x14;
            std::memset(name, 0, 0x14);
            name[0] = static_cast<char>('a' + t);
            name[1] = static_cast<char>('0' + i);
            std::strcpy(name + 2, ".tga");
        }
        names[6] = static_cast<char>(a[4] >> 8);
        names[7] = static_cast<char>(a[4]);
        return 0;
    }
    case 13: DelayedLoad_LoadTextures(); return 0;
    case 14: gei_TextureSettings.mFlags = static_cast<uint32_t>(a[1]); return 0;
    case 16:   // a[1] loads in the manager's table, with a[2 + i] references each
        gManager.mCount = a[1];
        for (int32_t i = 0; i < a[1]; ++i) {
            gManager.mLoads[i].mFolder = nullptr;
            gManager.mLoads[i].mName = nullptr;
            gManager.mLoads[i].mTexture = NewArray(1);
            gManager.mLoads[i].mReferences = a[2 + i];
        }
        return 0;
    case 17: {   // RemoveTexture of load a[1]'s texture (below 0: of a texture the table does not have)
        CTextureEntry* texture = a[1] >= 0 ? gManager.mLoads[a[1]].mTexture : NewArray(1);
        return gManager.RemoveTexture(texture);
    }
    case 18: gManager.SetLoadFlags(a[1]); return 0;
    }
    return -99;
}

// The state in the order of PpcSide.state in the Python module.
int32_t tpw_lm_state(int64_t* out) {
    int32_t n = 0;
    const auto record = [&](const void* pointer, const void* base, size_t stride) -> int64_t {
        if (pointer == nullptr) return 0;
        const auto offset = static_cast<const char*>(pointer) - static_cast<const char*>(base);
        return offset >= 0 && offset < static_cast<std::ptrdiff_t>(2 * stride) ? 1 + offset / static_cast<std::ptrdiff_t>(stride) : -1;
    };
    for (int32_t k = 0; k < 2; ++k) {
        const CsTexture* object = Object(k);
        out[n++] = record(object->mTexture, gTextures, sizeof(sTexture));
        out[n++] = record(object->mInfo, gInfos, sizeof(sTextureInfo));
        const int32_t label = lab(object->mEntries);
        out[n++] = label;
        out[n++] = object->mEntryCount;
        out[n++] = object->mReferences;
        const int32_t count = label > 0 && label < 1024 ? gListCounts[label] : 0;
        for (int32_t i = 0; i < 4; ++i) out[n++] = i < count ? lab(object->mEntries[i]) : -7;
        for (int32_t i = 0; i < 4; ++i) {
            const CTextureEntry* entry = i < count ? object->mEntries[i] : nullptr;
            out[n++] = entry != nullptr && lab(entry) > 0 ? entry->mFlags : 0;
        }
    }
    out[n++] = gManager.mCount;
    out[n++] = gManager.mLoadFlags;
    for (int32_t i = 0; i < 6; ++i) {
        const bool used = i < gManager.mCount;
        out[n++] = used ? lab(gManager.mLoads[i].mTexture) : -7;
        out[n++] = used ? gManager.mLoads[i].mReferences : -7;
    }
    out[n++] = gDelayedLoadCount;
    for (int32_t i = 0; i < 4; ++i) {
        out[n++] = i < gDelayedLoadCount ? record(gDelayedLoads[i], gObjectStore, sizeof gObjectStore[0]) : -7;
    }
    out[n++] = gei_TextureSettings.mFlags;
    return n;
}
}
