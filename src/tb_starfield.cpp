// tb_starfield.cpp range of the engine library (SimTheme Park 2.04b3): the sprite
// manager's start-up and shutdown. PARTIAL: the sprite classes and the rest of the range
// are still to do.
#include "tb_starfield.h"

#include <new>

#include "tb_error.h"
#include "tb_loader_dependencies.h"   // CTextureEntry

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

CSpriteManager::CSpriteManager() {
    mF2404 = 0;
    mF2408 = 0;
    mTextures = nullptr;
    mF240C = 0;
}

CSpriteManager::~CSpriteManager() { Close(); }

int32_t CSpriteManager::Close() {
    if (mTextures != nullptr) {
        delete[] mTextures;
        mTextures = nullptr;
    }
    return 1;
}
