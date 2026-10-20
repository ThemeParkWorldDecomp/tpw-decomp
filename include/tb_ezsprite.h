// tb_ezsprite.h: a minimal sprite manager on top of the engine's sprite classes (engine
// library, original tb_ezsprite.cpp, functions at 0x10008b24..0x10009650).
#pragma once
#include <cstdint>

#include "platform/d3d_types.h"

class CSpriteList;
struct SSpriteEngineData;
struct SSpriteGameData;

class CEZSprite {
public:
    CEZSprite();
    // Close(), as the original's destructor does.
    ~CEZSprite();

    // Up to maxInstances instances of up to maxSprites sprites (maxSprites 0 means as many
    // as instances); 0 when already initialised or out of memory.
    int32_t Init(int32_t maxInstances, int32_t maxSprites);
    int32_t Close();
    // Link / unlink the instance list into gei_MapWho (list 0x60).
    int32_t Enable();
    int32_t Disable();
    SSpriteEngineData* LoadSprite(char* name);
    bool DeleteSprite(void* sprite);
    SSpriteGameData* CreateSpriteInstance(void* sprite);
    bool SetSpritePosition(void* instance, D3DVECTOR position);
    bool DeleteSpriteInstance(void* instance);
    void* GetSpriteDetails(void* instance);

    uint32_t mFlags;                  // 0x00  bit 0: linked (Enable)
    CSpriteList* mList;               // 0x04
    SSpriteGameData** mInstances;     // 0x08  mList->Init(maxInstances)
    SSpriteEngineData** mSprites;     // 0x0c  gMalloc(maxSprites * 4)
    int32_t mMaxInstances;            // 0x10  0 until Init
    int32_t mMaxSprites;              // 0x14  not set by the constructor
    int32_t mSpriteCount;             // 0x18
    int32_t mInstanceCount;           // 0x1c
};

extern CEZSprite gei_EZSprite;   // engine data 0x10052d48 (0x20 bytes)
