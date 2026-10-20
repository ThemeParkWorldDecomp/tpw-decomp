// ESP_sprites_ESP_SwitchToFirstPersonSprites.cpp: the rest of HLSprite.cpp's sprite code (unit v5027):
//   0x1007cfc8 ESP_SwitchToFirstPersonSprites, 0x1007d280 ESP_SwitchToThirdPersonSprites (reload every sprite set's data
//   file with the other view's extension) and 0x1007ca9c ESP_AddHLSpriteFromScript (fills the sprite record embedded in a
//   script and links it into a cell's list).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ESP_sprites.h"
#include "Game.h"
#include "HLSprite.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPResource.h"
#include "TbDynamicStringTemplate.h"
#include "analyser.h"
#include "sprite_engine.h"

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

void ESP_SwitchToFirstPersonSprites() { SwitchSprites(0); }

void ESP_SwitchToThirdPersonSprites() { SwitchSprites(1); }

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
