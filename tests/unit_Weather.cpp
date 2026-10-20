// Weather scripts: real rain and sky, fake texture entries, random generator and timer.
#include <cstdlib>
#include <cstring>
#include <new>
namespace { void observe_allocation(std::size_t size, bool array, void* block); }
#define TPW_ALLOCATION_OBSERVER observe_allocation
#include "calllog.cpp"
#include "tb_rain.h"
#include "tb_sky.h"
#include "tb_textureentry.h"
#include "tb_error.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "tb_loaders.h"
#include "Wavelet.h"
using namespace calllog;

namespace {
int gQuiet;
struct Quiet {
    Quiet() { ++gQuiet; }
    ~Quiet() { --gQuiet; }
};
void observe_allocation(std::size_t size, bool array, void* block) {
    if (!on() || gQuiet) return;
    Quiet quiet;
    if (!array) std::memset(block, 0xa5, size);
    ev(array ? "new[]" : "new", {newlab(block)});
}
uint32_t gRandom;
uint32_t gRandomCalls;
uint32_t gPixels[256 * 256];
uint32_t gManagerStorage;
void* gOut;
const char* names[] = {nullptr, "weather:a.tga", "weather:b.tga", "weather:c.tga", "", "Park:LostKingdom"};
float fl(int32_t word) {
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}
D3DVECTOR vec(const int32_t* a) { return {fl(a[0]), fl(a[1]), fl(a[2])}; }
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    if (on() && !gQuiet && !ret("New", 1)) return nullptr;
    return operator new(size);
}

// Random is deliberately unlogged (one Init uses over 8,000 draws), but its state and
// call count are compared after each operation. It returns the MSL sequence on both sides.
int32_t msl_rand() {
    gRandom = gRandom * 0x41c64e6du + 0x3039u;
    ++gRandomCalls;
    return (gRandom >> 16) & 0x7fff;
}
uint64_t SamsUtilities::UTimer::GetAbsolute() {
    Quiet quiet;
    ev("Timer");
    return static_cast<uint64_t>(ret("Timer", 123456789));
}
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t level, int32_t code) {
    Quiet quiet;
    ev("Error", {level, code});
}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    Quiet quiet;
    ev("assert", {line}, {condition, file});
}
CSystem::CSystem() {}
CSystem gei_System;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CTextureEntry::CTextureEntry() {
    std::memset(static_cast<void*>(this), 0, sizeof *this);
}
CTextureEntry::~CTextureEntry() {
    Quiet quiet;
    ev("Entry.dtor");
}
int32_t CTextureEntry::Close() {
    Quiet quiet;
    uint32_t owners = 0;
    CTextureEntry* rain[] = {gei_Rain ? gei_Rain->mSnowTexture : nullptr,
                            gei_Rain ? gei_Rain->mLightningTexture : nullptr,
                            gei_Rain ? gei_Rain->mRainTexture : nullptr};
    CTextureEntry* sky[] = {gei_Sky.mSkyTexture, gei_Sky.mColorTexture, gei_Sky.mCylinderTexture};
    for (int i = 0; i < 3; ++i) {
        if (rain[i] == this) owners |= 1u << i;
        if (sky[i] == this) owners |= 1u << (3 + i);
    }
    for (int i = 0; i < 16; ++i) if (gei_Sky.mAdds[i].mTexture == this) owners |= 1u << (6 + i);
    ev("Entry.Close", {lab(this), owners, gei_Sky.mAddCount});
    return static_cast<int32_t>(ret("Entry.Close", 1));
}
void* CTextureEntry::GetData() {
    Quiet quiet;
    ev("Entry.GetData", {lab(this)});
    return gPixels;
}
CTextureLoadManager* gei_TLoadManager = reinterpret_cast<CTextureLoadManager*>(&gManagerStorage);
CTextureEntry* CTextureLoadManager::LoadFile(char* path) {
    int count;
    uint32_t flags;
    {
        Quiet quiet;
        ev("LoadFile", {lab(this), gei_Sky.mAddCount}, {path});
        count = static_cast<int>(ret("LoadFile", 1));
        flags = static_cast<uint32_t>(ret("Flags", 0xffffffffu));
    }
    if (count == 0) return nullptr;
    CTextureEntry* entries = new CTextureEntry[count];
    newlab(entries);
    entries->mFlags = flags;
    entries->mWidth = entries->mHeight = entries->mPitch = 32;
    return entries;
}
static char* filename(const char* name, int32_t flags) {
    Quiet quiet;
    ev("FileName", {flags}, {name});
    if (!ret("FileName", 1)) return nullptr;
    static char result[160];
    std::strcpy(result, "resolved/");
    std::strcat(result, name);
    return result;
}
TbFileNameFn gFileName = filename;

extern "C" {
void* tpw_weather_sky() { return &gei_Sky; }
void* tpw_weather_manager() { return gei_TLoadManager; }
void tpw_weather_reset(uint32_t seed) {
    Quiet quiet;
    Rain_Close();
    gei_Sky.Close();
    for (SSkyAdd& add : gei_Sky.mAdds) {
        if (add.mTexture != nullptr) delete[] add.mTexture;
    }
    std::memset(static_cast<void*>(&gei_Sky), 0xa5, sizeof gei_Sky);
    // Constructor does not set these pointers; batch 1 never dereferences them.
    gei_Sky.mSkyTexture = gei_Sky.mColorTexture = gei_Sky.mCylinderTexture = nullptr;
    for (SSkyAdd& add : gei_Sky.mAdds) add.mTexture = nullptr;
    gOut = reinterpret_cast<void*>(55);
    gei_System.mRenderer = 3;
    new (&gei_Sky) CSky;
    gRandom = seed;
    gRandomCalls = 0;
}
int64_t tpw_weather_op(int32_t op, const int32_t* a) {
    CRain* rain = gei_Rain;
    if (op >= 3 && op <= 12 && rain == nullptr) return -9;
    switch (op) {
    case 0: return Rain_Init();
    case 1: return Rain_Close();
    case 2:
        if (rain != nullptr) return -9;
        gei_Rain = new CRain;
        return 0;
    case 3: return rain->Init();
    case 4: return rain->Close();
    case 5: rain->SetRainType(a[0]);
        return 0;
    case 6: rain->SetLength(fl(a[0]));
        return 0;
    case 7: rain->SetRaindrops(a[0]);
        return 0;
    case 8: rain->SetDirection(vec(a));
        return 0;
    case 9: rain->SetDirection(fl(a[0]), fl(a[1]), fl(a[2]));
        return 0;
    case 10: rain->SetWindParameters(fl(a[0]), fl(a[1]));
        return 0;
    case 11: rain->SetVelocity(fl(a[0]));
        return 0;
    case 12: rain->LightningFlash();
        return 0;
    case 13:
        if (rain == nullptr) return -9;
        rain->SetSnowCubicParameters(fl(a[0]), fl(a[1]), fl(a[2]));
        return 0;
    case 14:
        gei_System.mRenderer = a[0];
        new (&gei_Sky) CSky;
        return 0;
    case 15: return gei_Sky.Init();
    case 16: gei_Sky.InitColors();
        return 0;
    case 17: return gei_Sky.Close();
    case 18: gei_Sky.SetSpherePos(vec(a));
        return 0;
    case 19: gei_Sky.SetSphereRadius(fl(a[0]));
        return 0;
    case 20: gei_Sky.ResetSpherePos();
        return 0;
    case 21: gei_Sky.SetupScrollDirection(a[0], vec(a + 1));
        return 0;
    case 22: gei_Sky.SetActiveLayers(a[0]);
        return 0;
    case 23: return lab(gei_Sky.GetSkyTEntry());
    case 24: return gei_Sky.GetPosition() == gei_Sky.mPosition;
    case 25: gei_Sky.~CSky();
        return 0;
    case 26: new (&gei_Sky) CSky;
        return 0; // original static initialiser
    case 27: { // Plant a texture array for a Close or an InitColors.
        CTextureEntry** slots[] = {rain ? &rain->mSnowTexture : nullptr,
                                  rain ? &rain->mLightningTexture : nullptr,
                                  rain ? &rain->mRainTexture : nullptr,
                                  &gei_Sky.mSkyTexture, &gei_Sky.mColorTexture, &gei_Sky.mCylinderTexture};
        CTextureEntry** slot = a[0] < 6 ? slots[a[0]] : &gei_Sky.mAdds[a[0] - 6].mTexture;
        if (slot == nullptr || *slot != nullptr) return -9;
        *slot = new CTextureEntry[a[1]];
        newlab(*slot);
        (*slot)->mWidth = a[2];
        (*slot)->mHeight = a[3];
        (*slot)->mPitch = a[4];
        (*slot)->mFlags = a[5];
        return 0;
    }
    case 28: { // A texture: the same BYTES on both sides (pixel data is the Mac's byte for byte).
        gei_TextureSettings.mFormat = a[0];
        const uint32_t seed = a[1];
        auto* bytes = reinterpret_cast<uint8_t*>(gPixels);
        if (a[0] == 2 || a[0] == 4) {
            for (uint32_t i = 0; i < 256 * 256; ++i) {
                const uint32_t value = (seed + i * 40503u) & 0xffffu;
                bytes[2 * i] = static_cast<uint8_t>(value >> 8);
                bytes[2 * i + 1] = static_cast<uint8_t>(value);
            }
        } else {
            for (uint32_t i = 0; i < 256 * 256; ++i) {
                const uint32_t value = seed + i * 2654435761u;
                bytes[4 * i] = static_cast<uint8_t>(value >> 24);
                bytes[4 * i + 1] = static_cast<uint8_t>(value >> 16);
                bytes[4 * i + 2] = static_cast<uint8_t>(value >> 8);
                bytes[4 * i + 3] = static_cast<uint8_t>(value);
            }
        }
        return 0;
    }
    case 29: // Vary the grid dimensions to exercise clipping branches.
        gei_Sky.mWidth = a[0];
        gei_Sky.mHeight = a[1];
        return 0;
    case 30: { // Destructor's positive delete-storage flag on the original.
        CSky* sky = new CSky;
        delete sky;
        return 0;
    }
    case 31: return rain->Load(a[0], const_cast<char*>(names[a[1]]));
    case 32: return rain->GameLoad();
    case 33: {
        CTextureEntry** slots[] = {&gei_Sky.mSkyTexture, &gei_Sky.mColorTexture, &gei_Sky.mCylinderTexture};
        return gei_Sky.Load(slots[a[0]], const_cast<char*>(names[a[1]]));
    }
    case 34: return gei_Sky.LoadAll(const_cast<char*>(names[a[0]]), const_cast<char*>(names[a[1]]),
                                  const_cast<char*>(names[a[2]]));
    case 35: return gei_Sky.LoadAdd(&gOut, const_cast<char*>(names[a[0]]), vec(a + 1), fl(a[4]), a[5], a[6]);
    case 36: gei_Sky.RemoveAdd(a[0]);
        return 0;
    case 37: return gei_Sky.RemoveAdd(a[0] == -1 ? reinterpret_cast<void*>(0x777) :
                                    a[0] == -2 ? nullptr : gei_Sky.mAdds[a[0]].mTexture);
    case 38: return gei_Sky.SkyLoad(const_cast<char*>(names[a[0]]));
    case 39: gei_Sky.mAddCount = a[0];
        return 0;
    case 40:
        for (int i = 0; i < 16; ++i) gei_Sky.mAdds[i].mF1C = static_cast<uint32_t>(a[0]) + i * 17u;
        return 0;
    }
    return -99;
}
int32_t tpw_weather_state(uint32_t* out) {
    Quiet quiet;
    uint32_t* start = out;
    const auto word = [&](uint32_t value) { *out++ = value; };
    // Each segment is a consecutive run of scalar fields; no host pointer or padding is copied.
    const auto words = [&](const void* first, std::size_t count) {
        std::memcpy(out, first, count * 4);
        out += count;
    };
    const auto entry = [&](CTextureEntry* e) {
        word(lab(e));
        if (e != nullptr) {
            word(e->mFlags);
            word(static_cast<uint16_t>(e->mWidth));
            word(static_cast<uint16_t>(e->mHeight));
            word(static_cast<uint16_t>(e->mPitch));
        }
    };
    word(gRandom);
    word(gRandomCalls);
    word(lab(gei_Rain));
    word(lab(gOut));
    if (const CRain* r = gei_Rain) {
        words(&r->mRainType, 0x3348 / 4);
        entry(r->mSnowTexture);
        entry(r->mLightningTexture);
        entry(r->mRainTexture);
        words(&r->mF3354, (0xa1cc - 0x3354) / 4);
    }
    words(gei_Sky.mCylinder, 0x1fd0 / 4);
    entry(gei_Sky.mSkyTexture);
    entry(gei_Sky.mColorTexture);
    entry(gei_Sky.mCylinderTexture);
    for (const SSkyAdd& add : gei_Sky.mAdds) {
        entry(add.mTexture);
        words(&add.mF04, 7);
    }
    words(&gei_Sky.mAddCount, 8);
    for (uint16_t index : gei_Sky.mIndices) word(index);
    for (uint16_t index : gei_Sky.mCylinderIndices) word(index);
    words(&gei_Sky.mActiveLayers, 19);
    return out - start;
}
}
