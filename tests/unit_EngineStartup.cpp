// Test entry points for the engine's subsystem start-up and shutdown (library
// unit_EngineStartup: tb_resource_display.cpp, tb_rendermemory.cpp, tb_hfield.cpp,
// tb_mapzoom.cpp, tb_starfield.cpp, tb_switch.cpp, Wavelet.cpp). A test runs a script of
// operations; after each one it reads the state of every subsystem object as a flat list
// of numbers, built in the same order as tests/test_EngineStartup.py builds the
// original's. The engine allocator, operator new and delete, the error handler and the
// classes the unit only destroys are fakes that record what they are asked.
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void observe_allocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER observe_allocation
#include "calllog.cpp"

#include "MapWho.h"
#include "Wavelet.h"
#include "platform/msl.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loader_dependencies.h"
#include "tb_particle.h"
#include "tb_proctexture.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_starfield.h"
#include "tb_switch.h"

using namespace calllog;

namespace {
// Fakes and the test's own bookkeeping allocate too; those allocations are not events.
int gQuiet = 0;
struct Quiet {
    Quiet() { ++gQuiet; }
    ~Quiet() { --gQuiet; }
};
bool recording() { return on() && gQuiet == 0; }

// Every operator new of the code under test is an event with a new label; the object
// starts filled with 0xa5, as the PowerPC side's does, so fields a constructor leaves
// alone compare. (calllog.cpp records the matching "delete" of a labelled block.)
void observe_allocation(std::size_t size, bool array, void* block) {
    if (!recording()) return;
    Quiet quiet;
    if (!array) std::memset(block, 0xa5, size);
    ev(array ? "new[]" : "new", {newlab(block)});
}
}  // namespace

// The unit allocates with new (std::nothrow); the library's default for that form does not
// come through calllog.cpp's operator new, so it is replaced here too.
// The "New" queue makes an allocation of the code under test fail (0), as the PowerPC
// side's operator new stub does, so the paths behind the original's NULL checks run.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (recording() && !ret("New", 1)) return nullptr;
    return operator new(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return operator new[](size); }

// ---- The engine allocator -----------------------------------------------------------------
namespace {

// A block may be handed out a few bytes into its allocation (the "Skew" queue, for the
// one request whose result the unit rounds up), so each block's real start is kept.
struct Block {
    void* block;
    void* base;
};
Block gBlocks[256];
int gBlockCount = 0;
char gMarker;   // stands for a function the game installs
char gLinks[8];   // stand for link bases of other objects

void* FakeMalloc(uint32_t size) {
    Quiet quiet;
    // The pointer-holding tables are larger on a 64-bit host; the log shows the Mac's size.
    uint32_t shown = size;
    if (size == CMapWho::kNodeCount * sizeof(SMapLinkType)) shown = 0x24000;
    if (size == CRenderMemory::kInfoCount * sizeof(SRenderLoadInfo)) shown = 0x38000;
    ev("Malloc", {shown});
    if (!ret("Malloc", 1)) return nullptr;
    const uint32_t skew = shown == 0x10000f ? static_cast<uint32_t>(ret("Skew", 0)) : 0;
    auto* base = static_cast<uint8_t*>(std::calloc(1, size + 16));
    uint8_t* block = base + skew;
    if (gBlockCount < 256) gBlocks[gBlockCount++] = {block, base};
    newlab(block);
    return block;
}

void FakeFree(void*& block) {
    Quiet quiet;
    ev("Free", {lab(block)});
    forget(block);
    for (int i = 0; i < gBlockCount; ++i) {
        if (gBlocks[i].block == block) {
            std::free(gBlocks[i].base);
            gBlocks[i] = gBlocks[--gBlockCount];
            break;
        }
    }
    block = nullptr;
}

}  // namespace

// ---- Fakes of what the unit calls and does not contain -----------------------------------
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) {
    Quiet quiet;
    ev("Error", {level, code});
}

CSystem::CSystem() {}
CSystem gei_System;

// Wavelet.cpp's default allocator reports through this; the tests install their own.
void DebugStr(const unsigned char*) {}

void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    Quiet quiet;
    ev("assert", {line}, {condition, file});
}

CProcTextureHandler::CProcTextureHandler() {}
CProcTextureHandler::~CProcTextureHandler() {
    Quiet quiet;
    ev("Proc.dtor", {lab(this)});
}
int32_t CProcTextureHandler::Close() {
    Quiet quiet;
    ev("Proc.Close", {lab(this)});
    return 1;
}

// Virtual members that are still to do; nothing here calls them.
int32_t CParticle::Prepare() { return 0; }
int32_t CWaveletImp::Decompress(uint32_t, uint32_t, bool, const uint8_t*, uint8_t*, float, float, float) { return 0; }
int32_t CWaveletImp::Decompress(int32_t, int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) {
    return 0;
}
int32_t CWaveletImp::Decompress(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, void*, void*, void*,
                                float, float, float, float) {
    return 0;
}
int32_t CWaveletImp::Decompress_v12(int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) {
    return 0;
}
int32_t CWaveletImp::Decompress_v13(int32_t, int32_t, int32_t, void*, void*, void*, float, float, float, float) {
    return 0;
}
// tb_init.cpp's: the procedural textures' scratch buffer, which InitWaveLoad points.
uint8_t* gProcTextureBuffer = nullptr;

CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {
    Quiet quiet;
    ev("Entry.dtor");
}

namespace {

int64_t* gOut;
int32_t gCount, gCapacity;
void put(int64_t value) {
    if (gCount < gCapacity) gOut[gCount] = value;
    ++gCount;
}
uint32_t bits(float value) {
    uint32_t b;
    std::memcpy(&b, &value, 4);
    return b;
}
float from_bits(uint32_t b) {
    float value;
    std::memcpy(&value, &b, 4);
    return value;
}
// A pointer into a block as an offset from the block's start, or -2 when either is not
// a live pointer of the test.
int64_t offset_in(const void* pointer, const void* block) {
    if (pointer == nullptr) return -1;
    if (block == nullptr || lab(block) <= 0) return -2;
    return static_cast<const uint8_t*>(pointer) - static_cast<const uint8_t*>(block);
}

void RenderMemoryState() {
    CRenderMemory* m = gei_RenderMemory;
    put(lab(m));
    if (m == nullptr) return;
    put(m->mLoadFlags);
    put(m->mCurrentRenderType);
    for (const CRenderMemoryType& pool : m->mPools) {
        put(lab(pool.mData));
        put(pool.mUsed);
        put(pool.mSize);
        put(lab(pool.mIndexData));
        put(pool.mIndexUsed);
        put(pool.mIndexSize);
    }
    put(lab(m->mInfos));
    put(m->mInfoCount);
    put(m->mRemembered);
    put(m->mNextDynamic);
    put(m->mNextStatic);
    put(lab(m->mScratchBlock));
    const bool live = m->mScratchBlock != nullptr && lab(m->mScratchBlock) > 0;
    put(live ? offset_in(m->mScratch, m->mScratchBlock) : -3);
    put(live ? m->mScratchNext - m->mScratch : -3);
    put(live ? m->mScratchEnd - m->mScratch : -3);
    // The current record: its index and where its two pointers stand in their pools.
    const bool records = m->mInfos != nullptr && lab(m->mInfos) > 0;
    const uintptr_t first = reinterpret_cast<uintptr_t>(m->mInfos);
    const uintptr_t current = reinterpret_cast<uintptr_t>(m->mCurrent);
    if (records && current >= first && current < first + CRenderMemory::kInfoCount * sizeof(SRenderLoadInfo)) {
        const int32_t index = static_cast<int32_t>(m->mCurrent - m->mInfos);
        put(index);
        const SRenderLoadInfo& info = *m->mCurrent;
        int64_t data = -4, indexData = -4;
        for (int p = 0; p < 2; ++p) {
            const CRenderMemoryType& pool = m->mPools[p];
            if (pool.mData != nullptr && info.mData >= pool.mData && info.mData <= pool.mData + 0x100000)
                data = p * 0x1000000 + (info.mData - pool.mData);
            if (pool.mIndexData != nullptr && info.mIndexData >= pool.mIndexData &&
                info.mIndexData <= pool.mIndexData + 0x20000)
                indexData = p * 0x1000000 + (info.mIndexData - pool.mIndexData);
        }
        put(info.mData == nullptr ? -1 : data);
        put(info.mIndexData == nullptr ? -1 : indexData);
        uint32_t firstWord;
        std::memcpy(&firstWord, info.mF00, 4);
        put(firstWord);
    } else {
        put(-5);
    }
    // Which records have something in their first word: Reset clears a range of them.
    int64_t marked = 0;
    if (records) {
        for (int32_t i = 0; i < CRenderMemory::kInfoCount; ++i) {
            uint32_t word;
            std::memcpy(&word, m->mInfos[i].mF00, 4);
            if (word != 0) marked += i + 1;
        }
    }
    put(marked);
}

// A pointer into the dynamic pool's vertex data as an offset; -1 for NULL, -2 when the
// pool is not there.
int64_t dynamic_offset(const uint8_t* pointer) {
    if (pointer == nullptr) return -1;
    const CRenderMemory* m = gei_RenderMemory;
    if (m == nullptr || m->mPools[1].mData == nullptr) return -2;
    return pointer - m->mPools[1].mData;
}

void ParticleAndWaveletState() {
    const CParticle* particle = gei_Particle;
    put(lab(particle));
    if (particle != nullptr) {
        put(particle->mF04);
        put(particle->mF08);
        put(particle->mGetEmitterBoundingBox != nullptr);
        put(particle->mInitParticleSystem != nullptr);
        put(particle->mCloseParticleSystem != nullptr);
        put(particle->mLoadLibrary != nullptr);
        put(particle->mBuildDrawingList != nullptr);
        put(particle->mFreeDrawingList != nullptr);
        put(particle->mGetEmitterParticleCount != nullptr);
    }
    put(gLinkBaseCount);
    const CMapLinkBase* own = particle != nullptr ? static_cast<const CMapLinkBase*>(particle) : nullptr;
    for (int32_t i = 0; i < gLinkBaseCount && i < 0x200; ++i) {
        put(gLinkBases[i] != nullptr && gLinkBases[i] == own ? 1 : lab(gLinkBases[i]));
    }
    put(gWaveletInstance != nullptr);
    if (gWaveletInstance != nullptr) {
        put(gWaveletInstance->GetRefCount());
        put(lab(dynamic_cast<CWaveletImp*>(gWaveletInstance)->mBuffer));
    }
    put(dynamic_offset(gWaveletWorkspace));
    put(gWaveLoadWavelet != nullptr);
    put(dynamic_offset(gProcTextureBuffer));
}

void MapWhoState() {
    CMapWho* m = gei_MapWho;
    put(lab(m));
    if (m == nullptr) return;
    put(lab(m->mNodes));
    put(m->mNodeCount);
    put(m->mNodesUsed);
    put(lab(m->mMap));
    for (auto value : m->mF14) put(lab(value));
    put(lab(m->mF30));
    put(lab(m->mF34));
    put(m->mWidth);
    put(m->mHeight);
    put(m->mF40);
    put(m->mF70);
    put(m->mF74);
    put(lab(reinterpret_cast<void*>(m->mFinalCallback)));
    // The free list: its length, its first entry, and whether each entry leads to the one
    // before it in the block.
    int64_t length = 0, firstIndex = -1, ordered = 1;
    if (m->mNodes != nullptr && lab(m->mNodes) > 0) {
        for (SMapLinkType* node = m->mFree; node != nullptr && length <= CMapWho::kNodeCount; node = node->mNext) {
            const int64_t index = node - m->mNodes;
            if (length == 0) firstIndex = index;
            if (node->mNext != nullptr && node->mNext != node - 1) ordered = 0;
            ++length;
        }
    }
    put(length);
    put(firstIndex);
    put(ordered);
}

}  // namespace

extern "C" {

// The address the test labels 9: what op 20 installs as the map's callback.
void* tpw_es_marker() { return &gMarker; }
// The addresses the test labels 20 + k: link bases for ops 44 and 45.
void* tpw_es_link(int32_t k) { return &gLinks[k]; }

// Recording must be off. Frees whatever the last script left and puts every global back
// to its start: no subsystem objects, the display filled with `words`, a silent
// allocator.
void tpw_es_reset(const uint32_t* displayWords) {
    Quiet quiet;
    gMalloc = FakeMalloc;
    gFree = FakeFree;
    if (gei_HField != nullptr) gei_HField->mProcTextures = nullptr;
    if (gei_SpriteManager != nullptr) gei_SpriteManager->mTextures = nullptr;
    if (gWaveletInstance != nullptr) {
        while (gWaveletInstance->Release() > 0) {
        }
    }
    gWaveletInstance = nullptr;
    gWaveLoadWavelet = nullptr;
    gWaveletWorkspace = nullptr;
    gProcTextureBuffer = nullptr;
    Particle_Close();
    gLinkBaseCount = 0;
    Gamut_Close();
    LightManager_Close();
    RenderList_Close();
    HField_Close();
    MapWho_Close();
    Switch_Close();
    SpriteManager_Close();
    RenderMemory_Close();
    std::memcpy(static_cast<void*>(&gei_Display), displayWords, sizeof gei_Display);
    std::memset(gSineTable, 0, sizeof gSineTable);
    for (float& value : GlobalDiffuse) value = 0.0f;
    gei_System.mRenderer = 3;
}

// One operation of a script; `a` are its integer arguments (float arguments as bits).
int64_t tpw_es_op(int32_t op, const int32_t* a) {
    switch (op) {
    case 0: CreateSineTable(); return 0;
    case 1: return Gamut_Init();
    case 2: return Gamut_Close();
    case 3: return LightManager_Init();
    case 4: return LightManager_Close();
    case 5: return RenderList_Init();
    case 6: return RenderList_Close();
    case 7: return HField_Init();
    case 8: return HField_Close();
    case 9: return MapWho_Init();
    case 10: return MapWho_Close();
    case 11: return Switch_Init();
    case 12: return Switch_Close();
    case 13: return SpriteManager_Init();
    case 14: return SpriteManager_Close();
    case 15: return RenderMemory_Init();
    case 16: {
        const int32_t result = RenderMemory_Close();
        if (result) {   // they pointed into the pool that is gone
            gWaveletWorkspace = nullptr;
            gProcTextureBuffer = nullptr;
        }
        return result;
    }
    case 17:
        if (gei_Gamut == nullptr) return -9;
        gei_Gamut->SetYClipper(from_bits(a[0]), from_bits(a[1]));
        return 0;
    case 18: {   // give the height field things to close: bit 0 a texture handler, bit 1 a block
        if (gei_HField == nullptr) return -9;
        if (a[0] & 1) {
            gei_HField->mProcTextures = new CProcTextureHandler;
        }
        if (a[0] & 2) gei_HField->mF12970 = gMalloc(64);
        return 0;
    }
    case 19:   // give the map its square table
        if (gei_MapWho == nullptr) return -9;
        gei_MapWho->mMap = static_cast<SMapLinkType**>(gMalloc(64));
        return 0;
    case 20:
        if (gei_MapWho == nullptr) return -9;
        gei_MapWho->SetFinalCallback(a[0] ? reinterpret_cast<void (*)()>(&gMarker) : nullptr);
        return 0;
    case 21:   // give the sprite manager a texture array of a[0] entries
        if (gei_SpriteManager == nullptr) return -9;
        gei_SpriteManager->mTextures = new CTextureEntry[a[0]];
        return 0;
    case 22:
        if (gei_RenderMemory == nullptr || gei_RenderMemory->mInfos == nullptr) return -9;
        gei_RenderMemory->Reset(a[0]);
        return 0;
    case 23: {
        CRenderMemory* m = gei_RenderMemory;
        if (m == nullptr || m->mScratch == nullptr) return -9;
        const uint8_t* block = m->GetVertexBufferPtr(a[0]);
        return block == nullptr ? -1 : block - m->mScratch;
    }
    case 24: {
        CRenderMemory* m = gei_RenderMemory;
        if (m == nullptr || m->mInfos == nullptr) return -9;
        return m->LoadInfo(a[0]) - m->mInfos;
    }
    case 25: {
        CRenderMemory* m = gei_RenderMemory;
        if (m == nullptr || m->mInfos == nullptr) return -9;
        return m->LoadInfo_CopyVDB(a[0]) - m->mInfos;
    }
    case 26: {   // as if geometry had been loaded: pool a[0] grows, its next record moves
        CRenderMemory* m = gei_RenderMemory;
        if (m == nullptr || m->mInfos == nullptr) return -9;
        m->mPools[a[0]].mUsed += a[1];
        m->mPools[a[0]].mIndexUsed += a[2];
        if (a[0] == 0) m->mNextStatic -= a[3];
        else m->mNextDynamic += a[3];
        return 0;
    }
    case 27: new (&gei_Display) CDisplay; return 0;
    case 28:
        gei_System.mRenderer = a[3];
        gei_Display.SetDisplay(a[0], a[1], a[2]);
        return 0;
    case 29: gei_Display.SetWindow(from_bits(a[0]), from_bits(a[1]), from_bits(a[2]), from_bits(a[3])); return 0;
    case 30: gei_Display.ResetWindow(); return 0;
    case 31: {
        SBitData c[4];
        for (int i = 0; i < 4; ++i) c[i] = {static_cast<uint32_t>(a[i * 3]), a[i * 3 + 1], a[i * 3 + 2]};
        gei_Display.mBitsPerPixel = a[12];
        gei_Display.SetBitPattern(c[0], c[1], c[2], c[3]);
        return 0;
    }
    case 32: gei_Display.SetZSpace(from_bits(a[0])); return 0;
    case 33: gei_Display.ZSetup(from_bits(a[0]), from_bits(a[1]), from_bits(a[2])); return 0;
    case 34: gei_Display.SetFogSpace(from_bits(a[0]), from_bits(a[1])); return 0;
    case 35: gei_Display.SetFogColor(static_cast<uint32_t>(a[0])); return 0;
    case 36:
        gei_Display.mRedMask = static_cast<uint32_t>(a[0]);
        gei_Display.mGreenMask = static_cast<uint32_t>(a[1]);
        gei_Display.mBlueMask = static_cast<uint32_t>(a[2]);
        gei_Display.SetMaxBits();
        return 0;
    case 37: return BitsSet(static_cast<uint32_t>(a[0]));
    case 38: return ShiftFactor(static_cast<uint32_t>(a[0]));
    case 39: gei_Display.OutputDebugInfo(); return 0;
    case 40: {   // every record's first word becomes nonzero
        CRenderMemory* m = gei_RenderMemory;
        if (m == nullptr || m->mInfos == nullptr) return -9;
        for (int32_t i = 0; i < CRenderMemory::kInfoCount; ++i) {
            const uint32_t word = 0x1000 + i;
            std::memcpy(m->mInfos[i].mF00, &word, 4);
        }
        return 0;
    }
    case 41: {   // the member, without the object going away
        if (gei_RenderMemory == nullptr) return -9;
        const int32_t result = gei_RenderMemory->Close();
        gWaveletWorkspace = nullptr;   // they pointed into the pool that is gone
        gProcTextureBuffer = nullptr;
        return result;
    }
    case 42: return Particle_Init();
    case 43: return Particle_Close();
    case 44: return RegisterLinkBase(reinterpret_cast<CMapLinkBase*>(&gLinks[a[0]]));
    case 45: return FreeLinkBase(reinterpret_cast<CMapLinkBase*>(&gLinks[a[0]]));
    case 46:
        if (gei_RenderMemory == nullptr || gei_RenderMemory->mInfos == nullptr) return -9;
        return InitWaveLoad();
    case 47: {
        // The last reference destroys the decompressor and leaves the engine's pointer to
        // it dangling; the test clears it so the state can still be read.
        const bool last = gWaveLoadWavelet != nullptr && gWaveLoadWavelet->GetRefCount() <= 1;
        const int32_t result = CloseWaveLoad();
        if (last) gWaveletInstance = nullptr;
        return result;
    }
    case 48:
        if (gei_RenderMemory == nullptr || gei_RenderMemory->mInfos == nullptr) return -9;
        return CWavelet::Instance() != nullptr;
    case 49:
        if (gWaveletInstance == nullptr) return -9;
        return gWaveletInstance->AddRef();
    case 50:
        if (gWaveletInstance == nullptr) return -9;
        return gWaveletInstance->GetRefCount();
    case 51: {
        if (gWaveletInstance == nullptr) return -9;
        const int32_t result = gWaveletInstance->Release();
        if (result < 1) {
            gWaveletInstance = nullptr;
            gWaveLoadWavelet = nullptr;
        }
        return result;
    }
    case 52:
        if (gei_Particle == nullptr) return -9;
        return gei_Particle->GetLinkType();
    case 53:
        if (gei_Particle == nullptr) return -9;
        return gei_Particle->GetData() != nullptr;
    }
    return -99;
}

// Every subsystem's state (see the module comment); returns how many numbers there are.
int32_t tpw_es_state(int64_t* out, int32_t capacity) {
    Quiet quiet;
    gOut = out;
    gCount = 0;
    gCapacity = capacity;

    put(lab(gei_Gamut));
    if (const CGamut* g = gei_Gamut) {
        put(bits(g->mYClipLow));
        put(bits(g->mYClipHigh));
        for (float value : g->mF3C) put(bits(value));
        for (int32_t value : g->mF54) put(value);
        put(g->mF6C);
        put(g->mFC0);
        put(g->mFC4);
        put(g->mFC8);
        put(g->mF1CC);
        put(g->mF1D0);
    }
    put(lab(gei_LightManager));
    if (const CLightManager* l = gei_LightManager) {
        put(l->mF00);
        put(l->mF04);
    }
    for (float value : GlobalDiffuse) put(bits(value));
    put(lab(gei_RenderList));
    if (gei_RenderList != nullptr) put(gei_RenderList->mCount);
    put(lab(gei_HField));
    if (const CHField* h = gei_HField) {
        put(lab(h->mF00));
        put(lab(h->mF04));
        put(h->mF20);
        put(h->mF24);
        put(h->mF128);
        put(h->mF12C);
        put(lab(h->mProcTextures));
        put(h->mF234);
        put(h->mF238);
        put(h->mF23C);
        put(h->mF240);
        put(h->mF2958);
        put(h->mF12968);
        put(lab(h->mF12970));
    }
    MapWhoState();
    put(lab(gei_Switch));
    if (gei_Switch != nullptr) put(gei_Switch->mF00);
    put(lab(gei_SpriteManager));
    if (const CSpriteManager* s = gei_SpriteManager) {
        put(s->mTextures == nullptr ? 0 : 1);
        put(s->mF2404);
        put(s->mF2408);
        put(s->mF240C);
    }
    RenderMemoryState();
    ParticleAndWaveletState();
    const auto* words = reinterpret_cast<const uint32_t*>(&gei_Display);
    for (size_t i = 0; i < sizeof gei_Display / 4; ++i) put(words[i]);
    for (float value : gSineTable) put(bits(value));
    return gCount;
}

}  // extern "C"
