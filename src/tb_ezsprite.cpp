// tb_ezsprite.cpp (decompiled from the engine library of SimTheme Park 2.04b3, functions at
// 0x10008b24..0x10009650; SError's inline constructor, emitted first in this range,
// belongs to tb_error.h).
//
// msl_assertion_failed calls are the original's failed asserts (platform/msl.h).
#include "tb_ezsprite.h"

#include "MapWho.h"
#include "Wavelet.h"
#include "platform/msl.h"
#include "tb_error.h"
#include "tb_starfield.h"

CEZSprite gei_EZSprite;

CEZSprite::CEZSprite()
    : mFlags(0), mList(nullptr), mInstances(nullptr), mSprites(nullptr), mMaxInstances(0), mSpriteCount(0),
      mInstanceCount(0) {}

CEZSprite::~CEZSprite() {
    Close();
}

int32_t CEZSprite::Init(int32_t maxInstances, int32_t maxSprites) {
    if (mMaxInstances != 0) {
        gei_ErrorHandler.Push(2, kErrInitTwice);
        msl_assertion_failed("0", "tb_ezsprite.cpp", 42);
        return 0;
    }
    mList = new CSpriteList;
    if (mList == nullptr) {   // dead: the runtime's new throws
        msl_assertion_failed("0", "tb_ezsprite.cpp", 50);
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return 0;
    }
    mList->mCount = 0;
    mInstances = mList->Init(maxInstances);
    if (mInstances == nullptr) {
        msl_assertion_failed("0", "tb_ezsprite.cpp", 60);
        Close();
        return 0;
    }
    int32_t sprites = maxSprites != 0 ? maxSprites : maxInstances;
    // The original asks for sprites << 2 bytes: four bytes per pointer on the Mac.
    mSprites = static_cast<SSpriteEngineData**>(
        gMalloc(static_cast<uint32_t>(static_cast<uint32_t>(sprites) * sizeof(SSpriteEngineData*))));
    if (mSprites == nullptr) {
        msl_assertion_failed("0", "tb_ezsprite.cpp", 70);
        Close();
        return 0;
    }
    mMaxInstances = maxInstances;
    mMaxSprites = sprites;
    return 1;
}

int32_t CEZSprite::Close() {
    Disable();
    if (mSprites != nullptr) {
        for (int32_t i = 0; i < mSpriteCount; i++) {
            mSprites[i]->Close();
            delete mSprites[i];
        }
        void* block = mSprites;   // gFree clears the pointer it is given
        gFree(block);
        mSprites = static_cast<SSpriteEngineData**>(block);
        mSpriteCount = 0;
    }
    if (mList != nullptr) {
        for (int32_t i = 0; i < mInstanceCount; i++) delete mInstances[i];
        delete mList;
        mList = nullptr;
        mInstanceCount = 0;
    }
    mInstances = nullptr;
    mMaxInstances = 0;
    return 1;
}

int32_t CEZSprite::Enable() {
    if (mFlags & 1) return 1;
    if (gei_MapWho != nullptr && gei_MapWho->Link(mList, 0x60) != 0) {
        mFlags |= 1;
        return 1;
    }
    return 0;
}

int32_t CEZSprite::Disable() {
    if (!(mFlags & 1)) return 1;
    mFlags &= ~1u;   // cleared even when the unlink fails
    if (gei_MapWho != nullptr && gei_MapWho->Unlink(mList, 0x60) != 0) return 1;
    return 0;
}

SSpriteEngineData* CEZSprite::LoadSprite(char* name) {
    if (mSpriteCount == mMaxSprites) {
        msl_assertion_failed("0", "tb_ezsprite.cpp", 167);
        return nullptr;
    }
    SSpriteEngineData* sprite = new SSpriteEngineData;
    if (sprite == nullptr) {   // dead: the runtime's new throws
        msl_assertion_failed("0", "tb_ezsprite.cpp", 191);
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return nullptr;
    }
    if (sprite->Init(name) == 0) {
        msl_assertion_failed("0", "tb_ezsprite.cpp", 184);
        delete sprite;
        return nullptr;
    }
    mSprites[mSpriteCount] = sprite;
    mSpriteCount++;
    return sprite;
}

bool CEZSprite::DeleteSprite(void* sprite) {
    int32_t i = mSpriteCount - 1;
    while (i >= 0 && mSprites[i] != sprite) i--;
    if (i < 0) {
        msl_assertion_failed("0", "tb_ezsprite.cpp", 214);
        return false;
    }
    SSpriteEngineData* data = static_cast<SSpriteEngineData*>(sprite);
    data->Close();
    delete data;
    mSpriteCount--;
    mSprites[i] = mSprites[mSpriteCount];   // the last one fills the hole
    return true;
}

SSpriteGameData* CEZSprite::CreateSpriteInstance(void* sprite) {
    SSpriteGameData* instance = new SSpriteGameData;
    if (instance == nullptr) {   // dead: the runtime's new throws
        msl_assertion_failed("0", "tb_ezsprite.cpp", 239);
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return nullptr;
    }
    instance->mF10 = 0x58000;
    instance->mF24 = -1;
    instance->mF2C = 10.0f;
    instance->mSprite = static_cast<SSpriteEngineData*>(sprite);
    mInstances[mInstanceCount] = instance;   // no check against mMaxInstances
    mInstanceCount++;
    mList->mCount = mInstanceCount;
    return instance;
}

bool CEZSprite::SetSpritePosition(void* instance, D3DVECTOR position) {
    int32_t i = mInstanceCount - 1;
    while (i >= 0 && mInstances[i] != instance) i--;
    if (i < 0) {
        msl_assertion_failed("0", "tb_ezsprite.cpp", 260);
        return false;
    }
    static_cast<SSpriteGameData*>(instance)->mPosition = position;
    return true;
}

bool CEZSprite::DeleteSpriteInstance(void* instance) {
    int32_t i = mInstanceCount - 1;
    while (i >= 0 && mInstances[i] != instance) i--;
    if (i < 0) {
        msl_assertion_failed("0", "tb_ezsprite.cpp", 282);
        return false;
    }
    delete static_cast<SSpriteGameData*>(instance);
    mInstanceCount--;
    mInstances[i] = mInstances[mInstanceCount];
    mList->mCount = mInstanceCount;
    return true;
}

void* CEZSprite::GetSpriteDetails(void* instance) {
    return instance;
}
