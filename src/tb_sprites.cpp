// tb_sprites.cpp: the sprite manager's frame members (engine library of SimTheme Park
// 2.04b3; the inventory files the range 0x10031604..0x10031f08 under tb_starfield.cpp, and
// the asserts name tb_spritelist.cpp). Construction and shutdown are in tb_starfield.cpp.
//
// Every frame the world adds the sprites it wants drawn (AddSprite, AddPartialSprite,
// AddList), and RenderSprites sorts them by texture and blending into a tree of kinds
// (gSpriteGroups) and draws one primitive per kind.
#include <cstring>

#include "platform/msl.h"
#include "tb_drawprimitive.h"
#include "tb_loaders.h"
#include "tb_resource_display.h"
#include "tb_starfield.h"
#include "tb_textureentry.h"
#include "tb_texturecache.h"

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

void CSpriteManager::Reset() {
    mF2404 = 0;
    mF2408 = 0;
    mF240C = 0;
    gei_TCache->Prepare(mTextures, 0.0f);
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

int32_t CSpriteManager::Render() {
    if (RenderSprites() == 0) return 0;
    if (RenderPartialSprites() == 0) return 0;
    return 1;
}

int32_t CSpriteManager::RenderPartialSprites() {
    for (int32_t i = mF2408; i-- > 0;) {
        if (mPartialSprites[i]->DrawPartial() == 0) return 0;
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
