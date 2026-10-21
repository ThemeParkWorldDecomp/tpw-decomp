// SpriteManager.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of SpriteManager code, named after it.

#include "MapWho.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "sprite_engine.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_loader_dependencies.h"   // CTextureEntry
#include "tb_loaders.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "Wavelet.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// tb_sprite.cpp, the engine's sprite files (engine library of SimTheme Park 2.04b3; the file
// name is the one in its assertions; functions at 0x1002da98..0x1002f398 and 0x1003108c..
// 0x10031378, with the RLE loader of tb_decode_rle.cpp at 0x10005104..0x10005368 and the
// wavelet sprite reader of tb_decode_wavelet.cpp at 0x10006260; the inventory files them
// under tb_starfield.cpp and tb_depthoffield.cpp): the sprite file reader, a sprite's engine
// data and texture entry, a sprite instance's used part, the list of instances, and the two
// loaders a sprite's texture unpacks from. The file format is described in
// include/sprite_engine.h.
//
// PARTIAL, by design of the unit: the sprite's TGA loader (Init(char*)), the partial texture
// generator and the sprite drawing are other units; CSpriteList::Close, Prepare, GetLinkType and
// GetData, and the unpacking members of CRLELoad are untranslated.
//
// Each `new` is the original's operator new[] / operator new followed by the constructor loop or
// constructor; a null answer from it is dead code here (the runtime's new throws) and is kept
// where the original tests it.
//
// msl_assertion_failed calls are the original's failed asserts (platform/msl.h).

// ---- CSpriteList --------------------------------------------------------------------------------------

CSpriteList::CSpriteList()
    : mCount(0), mInstances(nullptr), mCapacity(0), mFrameA(-1), mFrameB(0), mF18(0) {
    RegisterLinkBase(this);
}

CSpriteList::~CSpriteList() {
    Close();
    FreeLinkBase(this);
}

// tb_starfield.cpp range of the engine library (SimTheme Park 2.04b3): the sprite
// manager's start-up and shutdown. PARTIAL: the sprite classes and the rest of the range
// are still to do.

CSpriteManager* gei_SpriteManager = nullptr;

int32_t SpriteManager_Init() {
    if (gei_SpriteManager != nullptr) return 0;
    gei_SpriteManager = new (std::nothrow) CSpriteManager;
    if (gei_SpriteManager == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t SpriteManager_Close() {
    if (gei_SpriteManager == nullptr) return 0;
    delete gei_SpriteManager;
    gei_SpriteManager = nullptr;
    return 1;
}

SSpriteGameData** CSpriteList::Init(int32_t count) {
    mInstances = static_cast<SSpriteGameData**>(
        gMalloc(static_cast<uint32_t>(static_cast<uint32_t>(count) * sizeof(SSpriteGameData*))));
    if (mInstances == nullptr) return nullptr;
    mCapacity = count;
    return mInstances;
}

// The sprite classes' remaining members (engine library of SimTheme Park 2.04b3): CSpriteList's Close,
// Prepare, GetData and GetLinkType (0x10031408, 0x10031488, 0x10031fb0, 0x10031fe4; the original file is
// tb_spritelist.cpp by its assertions, the inventory files it under tb_starfield.cpp), SSpriteEngineData's
// Init(char*), SetUsedPart and the partial texture generator and destroyer (0x1002e810, 0x1002eb00,
// 0x1002ebb8, 0x1002ee28; tb_sprite.cpp) and CSpriteFileIO::Load(char*, int) (0x1002dea0). The rest of
// these classes is in src/tb_spritefileio.cpp.
//
// Each `new` is the original's operator new[] followed by the constructor loop; a null answer from it is
// dead code here (the runtime's new throws) and is kept where the original tests it.

// ---- CSpriteList ---------------------------------------------------------------------------------------

// Frees the instance array (the allocator's free clears the pointer) and forgets its size. The count of
// instances in use (mCount) is the owner's.
int32_t CSpriteList::Close() {
    if (mInstances != nullptr) gFree(reinterpret_cast<void*&>(mInstances));
    mCapacity = 0;
    return 1;
}

// Gives the sprite manager this list once per display frame: the two frame words of the display (the
// frame count and the word a flip zeroes) are kept here and compared on the next call. A list the
// manager refused (its table is full) is remembered all the same.
int32_t CSpriteList::Prepare() {
    const int32_t frame = gei_Display.mFrameCount;
    const int32_t flipped = gei_Display.mF84;
    if (mFrameA == frame && mFrameB == flipped) return 1;
    const int32_t result = gei_SpriteManager->AddList(this);
    mFrameA = frame;
    mFrameB = flipped;
    return result;
}

CSpriteManager::CSpriteManager() {
    mF2404 = 0;
    mF2408 = 0;
    mTextures = nullptr;
    mF240C = 0;
}

CSpriteManager::~CSpriteManager() { Close(); }

// tb_sprites.cpp: the sprite manager's frame members (engine library of SimTheme Park
// 2.04b3; the inventory files the range 0x10031604..0x10031f08 under tb_starfield.cpp, and
// the asserts name tb_spritelist.cpp). Construction and shutdown are in tb_starfield.cpp.
//
// Every frame the world adds the sprites it wants drawn (AddSprite, AddPartialSprite,
// AddList), and RenderSprites sorts them by texture and blending into a tree of kinds
// (gSpriteGroups) and draws one primitive per kind.

SSpriteGroup gSpriteGroups[0x800];
SSpriteGameData* gHotSprite = nullptr;

namespace {

// Display flags RenderSprites tests (names invented).
constexpr uint32_t kDisplayDrawEach = 0x00000080;   // draw the sprites one by one, in the order added
constexpr uint32_t kDisplayNoShadows = 0x00020000;  // no blob shadows
constexpr uint32_t kDisplayBlobShadows = 0x00040000;  // one blob shadow texture for all of them

// Sprite flags (SSpriteGameData::mF10, names invented) that make up the blending key.
constexpr uint32_t kSpriteOwnBlend = 0x00000004;    // the key is fixed (below), not taken from the texture
constexpr uint32_t kSpriteAdditive = 0x00002000;    // with kSpriteOwnBlend: the second fixed key
constexpr int32_t kBlendOwn = 0x200522;
constexpr int32_t kBlendOwnAdditive = 0x200552;

// The flags that add a bit to the key, and the bit.
struct KeyBit {
    uint32_t flag;
    uint32_t bit;
};
constexpr KeyBit kKeyBits[] = {
    {0x00200000, 0x00200000},
    {0x00020000, 0x00020000},
    {0x00004000, 0x00000800},
    {0x00010000, 0x08000000},
};

// The texture entry's flag that says it is on a cache page.
constexpr uint32_t kTextureOnPage = 0x00001000;

}  // namespace

int32_t CSpriteManager::LoadBlobShadow(char* path) {
    if (path == nullptr) msl_assertion_failed("filename", "tb_spritelist.cpp", 0xd0);
    if (mTextures != nullptr) {
        delete[] mTextures;
        mTextures = nullptr;
    }
    mTextures = gei_TLoadManager->LoadFile(path);
    // The texture gets flag 0x40000 and flag 0x40 in place of flags 0x80 and 0x40 as they were.
    if (mTextures != nullptr) mTextures->mFlags = ((mTextures->mFlags | 0x40000) & 0xffffff3fu) | 0x40;
    return mTextures != nullptr;
}

int32_t CSpriteManager::Close() {
    if (mTextures != nullptr) {
        delete[] mTextures;
        mTextures = nullptr;
    }
    return 1;
}

void CSpriteManager::Reset() {
    mF2404 = 0;
    mF2408 = 0;
    mF240C = 0;
    gei_TCache->Prepare(mTextures, 0.0f);
}

int32_t CSpriteManager::Render() {
    if (RenderSprites() == 0) return 0;
    if (RenderPartialSprites() == 0) return 0;
    return 1;
}

int32_t CSpriteManager::AddSprite(SSpriteGameData* sprite) {
    if (mF2404 >= kMaxSprites) return 0;
    mSprites[mF2404++] = sprite;
    return 1;
}

int32_t CSpriteManager::AddPartialSprite(SSpriteGameData* sprite) {
    if (mF2408 >= kMaxSprites) return 0;
    mPartialSprites[mF2408++] = sprite;
    return 1;
}

int32_t CSpriteManager::AddList(CSpriteList* list) {
    if (mF240C >= kMaxLists) return 0;
    mLists[mF240C++] = list;
    return 1;
}

int32_t CSpriteManager::Prepare() {
    for (int32_t i = mF240C; i-- > 0;) {
        CSpriteList* list = mLists[i];
        for (int32_t j = list->mCount; j-- > 0;) list->mInstances[j]->Prepare();
    }
    return 1;
}

int32_t CSpriteManager::RenderSprites() {
    gHotSprite = nullptr;
    uint32_t displayFlags = gei_Display.mFlags;
    if ((displayFlags & kDisplayDrawEach) != 0) {
        const int32_t count = mF2404;
        if ((displayFlags & kDisplayNoShadows) == 0) {
            for (int32_t i = count; i-- > 0;) {
                if (mSprites[i]->DrawRegular() == 0) return 0;
                if (mSprites[i]->DrawRegularShadow() == 0) return 0;
            }
        } else {
            for (int32_t i = count; i-- > 0;) {
                if (mSprites[i]->DrawRegular() == 0) return 0;
            }
        }
        return 1;
    }

    // Sort the sprites, last added first, into a tree of kinds: texture page, then blending
    // key. Inside a kind the one with the largest depth comes first.
    const int32_t count = mF2404;
    std::memset(gSpriteGroups, 0xff, sizeof gSpriteGroups);
    SSpriteGroup* const root = gSpriteGroups;
    SSpriteGroup* end = root;
    // Quirk: the original's blending key is a local that is only set for a sprite with
    // kSpriteOwnBlend. For the others GetBlendingMode is called and its answer dropped, so
    // the key is the one the sprite before left, and each sprite only ever adds bits to it.
    // The first sprite starts from whatever register the caller left (0 here).
    int32_t blend = 0;
    const uintptr_t none = ~static_cast<uintptr_t>(0);
    for (int32_t i = count; i-- > 0;) {
        SSpriteGameData* sprite = mSprites[i];
        SSpriteGroup* group = root;
        const uint32_t spriteFlags = static_cast<uint32_t>(sprite->mF10);
        if ((spriteFlags & kSpriteOwnBlend) != 0) {
            blend = (spriteFlags & kSpriteAdditive) != 0 ? kBlendOwnAdditive : kBlendOwn;
        } else {
            sprite->mSprite->mTexture->GetBlendingMode();
        }
        for (const KeyBit& add : kKeyBits) {
            if ((spriteFlags & add.flag) != 0) blend = static_cast<int32_t>(static_cast<uint32_t>(blend) | add.bit);
        }
        const CTextureEntry* texture = sprite->mSprite->mTexture;
        const uintptr_t page = (texture->mFlags & kTextureOnPage) != 0 ? reinterpret_cast<uintptr_t>(texture->mTexture) : 0;
        sprite->mNext = nullptr;
        bool placed = false;
        if (end == root) {
            // The first sprite makes the root.
            root->mFirst = sprite;
            placed = true;
            ++end;
            root->mPage = page;
            root->mBlend = blend;
        }
        while (!placed) {
            const uintptr_t groupPage = group->mPage;
            if (groupPage == none) {
                group->mFirst = sprite;
                placed = true;
                group->mPage = page;
                group->mBlend = blend;
            } else if (groupPage == page && group->mBlend == blend) {
                SSpriteGameData* head = group->mFirst;
                SSpriteGameData* before = nullptr;
                SSpriteGameData* after = head;
                while (after != nullptr && sprite->mDepth < after->mDepth) {
                    before = after;
                    after = after->mNext;
                }
                if (before != nullptr) {
                    sprite->mNext = after;
                    before->mNext = sprite;
                } else {
                    sprite->mNext = head;
                    group->mFirst = sprite;
                }
                placed = true;
            } else if (groupPage > page || (groupPage == page && group->mBlend > blend)) {
                if (reinterpret_cast<uintptr_t>(group->mBelow) == none) {
                    group->mBelow = end;
                    ++end;
                }
                group = group->mBelow;
            } else {
                if (reinterpret_cast<uintptr_t>(group->mAbove) == none) {
                    group->mAbove = end;
                    ++end;
                }
                group = group->mAbove;
            }
        }
    }

    // Draw one primitive per kind, in the order the kinds were made.
    gei_DrawPrimitive->mCurrent = nullptr;
    for (SSpriteGroup* group = root; group < end; ++group) {
        if (group->mFirst == nullptr) continue;
        for (SSpriteGameData* sprite = group->mFirst; sprite != nullptr; sprite = sprite->mNext) {
            if (sprite->AddRegular() == 0) return 0;
        }
        if (gei_DrawPrimitive->ExecuteCurrentPrimitive(-1.0f) == 0) return 0;
    }

    // Then the shadows.
    displayFlags = gei_Display.mFlags;
    if ((displayFlags & kDisplayNoShadows) != 0) return 1;
    if ((displayFlags & kDisplayBlobShadows) != 0) {
        // One primitive for all of them, with the blob shadow texture.
        if (mTextures != nullptr) {
            for (SSpriteGroup* group = root; group < end; ++group) {
                for (SSpriteGameData* sprite = group->mFirst; sprite != nullptr; sprite = sprite->mNext) {
                    if (sprite->AddRegularShadow(mTextures) == 0) return 0;
                }
            }
            if (gei_DrawPrimitive->ExecuteCurrentPrimitive(-1.0f) == 0) return 0;
        }
        return 1;
    }
    for (SSpriteGroup* group = root; group < end; ++group) {
        if (group->mFirst == nullptr) continue;
        for (SSpriteGameData* sprite = group->mFirst; sprite != nullptr; sprite = sprite->mNext) {
            if (sprite->AddRegularShadow(nullptr) == 0) return 0;
        }
        // A kind's shadows wait for the next kind's when it has the same texture page.
        if (group + 1 == end || group->mPage != group[1].mPage) {
            if (gei_DrawPrimitive->ExecuteCurrentPrimitive(-1.0f) == 0) return 0;
        }
    }
    return 1;
}

int32_t CSpriteManager::RenderPartialSprites() {
    for (int32_t i = mF2408; i-- > 0;) {
        if (mPartialSprites[i]->DrawPartial() == 0) return 0;
    }
    return 1;
}

void* CSpriteList::GetData() { return nullptr; }

// The kind of link the map lists hold it as (3 for sprite lists).
int32_t CSpriteList::GetLinkType() { return 3; }
