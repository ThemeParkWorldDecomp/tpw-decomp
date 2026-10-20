// extern "C" wrappers for tests/test_tb_ezsprite.py (ctypes): engine tb_ezsprite.cpp.
//
// The CEZSprite under test lives in static storage (or is gei_EZSprite); its pointer
// fields are read and written through accessors. Sprite objects for a case's starting
// state are made with new (their placeholder constructors log calls the test discards).
#include <cstring>
#include <new>

#include "tb_ezsprite.h"
#include "tb_starfield.h"

namespace {
alignas(CEZSprite) unsigned char gStore[sizeof(CEZSprite)];
}

extern "C" {

CEZSprite* tpw_ez_store() { return reinterpret_cast<CEZSprite*>(gStore); }
CEZSprite* tpw_ez_global() { return &gei_EZSprite; }

void tpw_ez_set(CEZSprite* e, uint32_t flags, void* list, void* instances, void* sprites, const int32_t* counts) {
    e->mFlags = flags;
    e->mList = static_cast<CSpriteList*>(list);
    e->mInstances = static_cast<SSpriteGameData**>(instances);
    e->mSprites = static_cast<SSpriteEngineData**>(sprites);
    e->mMaxInstances = counts[0];
    e->mMaxSprites = counts[1];
    e->mSpriteCount = counts[2];
    e->mInstanceCount = counts[3];
}
void tpw_ez_get(const CEZSprite* e, uint32_t* flags, void** ptrs, int32_t* counts) {
    *flags = e->mFlags;
    ptrs[0] = e->mList;
    ptrs[1] = e->mInstances;
    ptrs[2] = e->mSprites;
    counts[0] = e->mMaxInstances;
    counts[1] = e->mMaxSprites;
    counts[2] = e->mSpriteCount;
    counts[3] = e->mInstanceCount;
}

void* tpw_new_list() { return new CSpriteList; }
void* tpw_new_sprite() { return new SSpriteEngineData; }
void* tpw_new_instance() { return new SSpriteGameData; }
void* tpw_list_base(void* list) { return static_cast<CMapLinkBase*>(static_cast<CSpriteList*>(list)); }
int32_t tpw_list_count(void* list, int32_t value, int32_t write) {
    CSpriteList* l = static_cast<CSpriteList*>(list);
    if (write != 0) l->mCount = value;
    return l->mCount;
}
// SSpriteGameData fields CEZSprite writes: mF10, mF24, mSprite, mF2C, mPosition.
void tpw_instance_get(const void* p, int32_t* ints, void** sprite, float* floats) {
    const SSpriteGameData* g = static_cast<const SSpriteGameData*>(p);
    ints[0] = g->mF10;
    ints[1] = g->mF24;
    *sprite = g->mSprite;
    floats[0] = g->mF2C;
    floats[1] = g->mPosition.x;
    floats[2] = g->mPosition.y;
    floats[3] = g->mPosition.z;
}
void tpw_instance_set(void* p, const int32_t* ints, void* sprite, const float* floats) {
    SSpriteGameData* g = static_cast<SSpriteGameData*>(p);
    g->mF10 = ints[0];
    g->mF24 = ints[1];
    g->mSprite = static_cast<SSpriteEngineData*>(sprite);
    g->mF2C = floats[0];
    g->mPosition = D3DVECTOR{floats[1], floats[2], floats[3]};
}

void tpw_CEZSprite_ctor(void* storage) { new (storage) CEZSprite(); }
int32_t tpw_CEZSprite_dtor(CEZSprite* e, int16_t flag) {
    e->~CEZSprite();
    return flag > 0 ? 1 : 0;
}
int32_t tpw_CEZSprite_Init(CEZSprite* e, int32_t a, int32_t b) { return e->Init(a, b); }
int32_t tpw_CEZSprite_Close(CEZSprite* e) { return e->Close(); }
int32_t tpw_CEZSprite_Enable(CEZSprite* e) { return e->Enable(); }
int32_t tpw_CEZSprite_Disable(CEZSprite* e) { return e->Disable(); }
void* tpw_CEZSprite_LoadSprite(CEZSprite* e, char* name) { return e->LoadSprite(name); }
int32_t tpw_CEZSprite_DeleteSprite(CEZSprite* e, void* s) { return e->DeleteSprite(s) ? 1 : 0; }
void* tpw_CEZSprite_CreateSpriteInstance(CEZSprite* e, void* s) { return e->CreateSpriteInstance(s); }
int32_t tpw_CEZSprite_SetSpritePosition(CEZSprite* e, void* i, const float* v) {
    return e->SetSpritePosition(i, D3DVECTOR{v[0], v[1], v[2]}) ? 1 : 0;
}
int32_t tpw_CEZSprite_DeleteSpriteInstance(CEZSprite* e, void* i) { return e->DeleteSpriteInstance(i) ? 1 : 0; }
void* tpw_CEZSprite_GetSpriteDetails(CEZSprite* e, void* i) { return e->GetSpriteDetails(i); }
void tpw_sinit_tb_ezsprite() { new (&gei_EZSprite) CEZSprite(); }

}  // extern "C"
