// InterfaceDrawObjectSprite.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceDrawObjectSprite code, named after it.

#include "MapWho.h"
#include "ui_DrawObject.h"

// InterfaceDrawObjectSprite (main app, 0x1018c978..0x1018d0bc; the inventory files it under ui_textmanager.cpp): the draw object that is
// a surface of the engine's sprite system. See include/ui_DrawObject.h for the class. The engine's sprite surfaces (SSpriteEngineData,
// SSpriteGameData, CSpriteList) are in the engine library; the map's lists are CMapWho's.

// The list of the map the sprite lists of the interface are linked into while shown.
namespace {
constexpr int32_t kSpriteMapList = 0x30;
}

// The members are made in the order the Mac's constructor makes them (the sprite's game data, the surface, the list). The flags
// 0x30000 and the scale 1.0 are the game data's words at Mac +0x10 and +0x2c; the sprite is the list's only instance.
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {
    mCreated = 0;
    mInstances = mList.Init(1);
    mSprite.mF10 = 0x30000;
    mSprite.mF2C = 1.0f;
    mSprite.mSprite = &mEngineData;
    mSprite.SetUsedPosition(0, 0);
    mInstances[0] = &mSprite;
    mList.mCount = 1;
    mPreDraw = nullptr;
    mF50 = 0;
}

// (The list and the surface are destroyed by the members' destructors, in that order, before the base.)
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}

void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) { mEngineData.RefreshPartialTexture(); }

// Makes the surface the size Change worked out, once.
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {
    if (mCreated == 0) {
        mEngineData.Init(mPixelWidth, mPixelHeight);
        mSprite.SetUsedSize(mPixelWidth, mPixelHeight);
        mCreated = 1;
    }
}

void InterfaceDrawObjectSprite::Refresh() { mEngineData.RefreshPartialTexture(); }

void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}

void InterfaceDrawObjectSprite::SetPreDrawCallback(int32_t (*callback)(TbRenderArea&)) { mPreDraw = callback; }

namespace {

// The compiler's code for dividing the rectangle's width in the real screen's pixels: (screen width * the width) / 2048, truncated
// toward zero, in 32 bits.
int16_t ScaledWidth(int32_t screen, int32_t width) {
    const int32_t product = screen * width;
    int32_t quotient = product >> 11;
    if (product < 0 && (product & 0x7ff) != 0) quotient += 1;
    return static_cast<int16_t>(quotient);
}

// (screen height * the height) / 1536 as the compiler's multiply-high and shifts do it, truncated toward zero.
int16_t ScaledHeight(int32_t screen, int32_t height) {
    const int32_t product = screen * height;
    const int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * product) >> 32);
    int32_t quotient = high >> 8;
    quotient += static_cast<int32_t>(static_cast<uint32_t>(quotient) >> 31);
    return static_cast<int16_t>(quotient);
}

}  // namespace

// Nothing for a NULL record or no flags. Flag 0x10 takes the rectangle from the record (the first time, also the surface's size
// in real pixels and the surface itself); before that nothing else is done. Flag 2 sets the depth (at least 1), flag 4 the word
// at 0x2c; any of those three places the sprite: x from the left, y from the BOTTOM, z from the depth (only when flag 0x10 or 2 is
// among them: flag 4 alone stores nothing).
void InterfaceDrawObjectSprite::Change(uint32_t flags, DOChange* change) {
    if (change == nullptr || flags == 0) return;
    const bool newRect = (flags & 0x10) != 0;
    if (newRect && mCreated == 0) {
        mRect.mLeft = change->mRect->mLeft;
        mRect.mTop = change->mRect->mTop;
        mRect.mRight = change->mRect->mRight;
        mRect.mBottom = change->mRect->mBottom;
        mPixelWidth = ScaledWidth(gUIRealScreenWidth, mRect.mRight - mRect.mLeft);
        mPixelHeight = ScaledHeight(gUIRealScreenHeight, mRect.mBottom - mRect.mTop);
        CreateTextures(nullptr);
    } else if (mCreated == 0) {
        return;
    }
    bool placed = false;
    if (newRect) {
        placed = true;
        mRect.mLeft = change->mRect->mLeft;
        mRect.mTop = change->mRect->mTop;
        mRect.mRight = change->mRect->mRight;
        mRect.mBottom = change->mRect->mBottom;
    }
    if ((flags & 2) != 0) {
        mDepth = change->mDepth;
        if (mDepth < 1) mDepth = 1;
        placed = true;
    }
    if ((flags & 4) != 0) {
        placed = true;
        mF2C = change->mF08;
    }
    if (placed && (flags & 0x12) != 0) {
        const float left = static_cast<float>(mRect.mLeft);
        const float bottom = static_cast<float>(mRect.mBottom);
        const float depth = static_cast<float>(mDepth);
        mSprite.mPosition.x = (2.0f * left) / 2048.0f - 1.0f;
        mSprite.mPosition.y = 1.0f - (2.0f * bottom) / 1536.0f;
        mSprite.mPosition.z = 1.0f - depth / 40.0f;
    }
}

// Links the sprite list into the map's list while shown. Does nothing until the surface exists, and then only when the state changes
// (the state is kept in the base's mShown).
void InterfaceDrawObjectSprite::Show(int16_t shown) {
    if (mCreated != 0 && mShown != shown) {
        CMapLinkBase* link = &mList;
        if (shown != 0) {
            gei_MapWho->Link(link, kSpriteMapList);
        } else {
            gei_MapWho->Unlink(link, kSpriteMapList);
        }
        mShown = shown;
    }
}

int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t* a, int32_t* b) {
    if (mCreated == 0 || a == nullptr || b == nullptr) return 0;
    *a = mEngineData.mWords.mWidth;
    *b = mEngineData.mWords.mHeight;
    return static_cast<int32_t>(reinterpret_cast<intptr_t>(mEngineData.mWords.mPixels));
}
