#include <cstdlib>
#include <cstring>
#include <new>
#include "TPOptions.h"
#include "OptionsBalance.h"
#include "Text.h"
#include "TimeSig.h"

namespace {
void font_seed(InterfaceFontWrapper* f, const uint32_t* w) {
    f->mF00 = w[0] >> 16; f->mF02 = w[0]; f->mF04 = w[1] >> 16; f->mHeight = int16_t(w[1]);
    f->mBitmap = reinterpret_cast<uint8_t*>(uintptr_t(w[2])); f->mID = int32_t(w[3]);
    f->mFont = reinterpret_cast<TbIFont*>(uintptr_t(w[4]));
}
void font_read(const InterfaceFontWrapper* f, uint32_t* w) {
    w[0] = (uint32_t(f->mF00) << 16) | f->mF02;
    w[1] = (uint32_t(f->mF04) << 16) | uint16_t(f->mHeight);
    w[2] = uint32_t(reinterpret_cast<uintptr_t>(f->mBitmap)); w[3] = uint32_t(f->mID);
    w[4] = uint32_t(reinterpret_cast<uintptr_t>(f->mFont));
}
void text_read(CText* p, uint32_t* w) {
    w[0] = p->mInitialised;
    w[1] = p->mResolutionGroup;
    w[2] = p->mLanguageId;
    for (int i = 0; i < 3; ++i) w[3 + i] = uint32_t(p->mResources.State(i));
    for (int i = 0; i < 52; ++i) font_read(&p->mFonts[i], w + 6 + i * 5);
    for (int i = 0; i < 19; ++i) w[266 + i] = uint32_t(p->mRenderParams.mState[i]);
    for (int i = 0; i < 23; ++i) w[285 + i] = uint32_t(reinterpret_cast<uintptr_t>(p->mResourcePointers[i]));
}
}
extern "C" {
void* tpw_Font_create(const uint32_t* w) {
    auto* p = static_cast<InterfaceFontWrapper*>(std::calloc(1, sizeof(InterfaceFontWrapper)));
    font_seed(p, w);
    return p;
}
void tpw_Font_read(InterfaceFontWrapper* p, uint32_t* w) { font_read(p, w); }
void tpw_Font_release(void* p) { std::free(p); }
void tpw_Font_ctor(InterfaceFontWrapper* p) { new (p) InterfaceFontWrapper; }
void tpw_Font_SetID(InterfaceFont* p, int32_t id) { p->SetID(id); }
void* tpw_Text_create(const uint32_t* w) {
    auto* p = static_cast<CText*>(std::calloc(1, sizeof(CText)));
    new (p) CText;
    p->mInitialised = w[0];
    p->mResolutionGroup = w[1];
    p->mLanguageId = w[2];
    for (int i = 0; i < 3; ++i) p->mResources.SetState(i, w[3 + i]);
    for (int i = 0; i < 52; ++i) font_seed(&p->mFonts[i], w + 6 + i * 5);
    for (int i = 0; i < 19; ++i) p->mRenderParams.mState[i] = w[266 + i];
    for (int i = 0; i < 23; ++i) p->mResourcePointers[i] = reinterpret_cast<void*>(uintptr_t(w[285 + i]));
    return p;
}
void* tpw_Text_subobject(CText* p, int i) {
    if (i == 52) return &p->mRenderParams;
    if (i == 53) return &p->mResources;
    return &p->mFonts[i];
}
void* tpw_Text_destroy(CText* p, int16_t flag, uint32_t* out) {
    p->~CText(); text_read(p, out);
    if (flag > 0) ::operator delete(p);
    return p;
}
void tpw_Text_release(void* p) { std::free(p); }
// The worlds are raw storage: CWorld has a constructor (src/World_init.cpp, unit w113) that the shim does not link.
alignas(CWorld) static unsigned char testGlobalWorldBytes[sizeof(CWorld)], testReturnedWorldBytes[sizeof(CWorld)];
static CWorld& testGlobalWorld = *reinterpret_cast<CWorld*>(testGlobalWorldBytes);
static CWorld& testReturnedWorld = *reinterpret_cast<CWorld*>(testReturnedWorldBytes);
extern CWorld* tpw_batch3_world;
void tpw_TimeSig_worlds(uint32_t global, uint32_t returned) {
    testGlobalWorld.mTurn = global; testReturnedWorld.mTurn = returned;
    gWorld = &testGlobalWorld; tpw_batch3_world = &testReturnedWorld;
}
uint32_t tpw_TimeSig_world_read(int i) { return i ? testReturnedWorld.mTurn : testGlobalWorld.mTurn; }
int32_t tpw_TimeSig_Serialise(CTimeSig* p, void* f, int32_t save, int32_t ver, uint32_t flags) {
    return p->Serialise(f, save, ver, flags);
}
void tpw_TimeSig_SetTimeSig(CTimeSig* p) { p->SetTimeSig(); }
int32_t tpw_TimeSig_TurnsSinceLastUpdate(const CTimeSig* p) { return p->TurnsSinceLastUpdate(); }
uint32_t tpw_TimeSig_GetTimeSecond(const CTimeSig* p) { return p->GetTimeSecond(); }
int32_t tpw_TimeSig_SecondsSinceLastUpdate(const CTimeSig* p) { return p->SecondsSinceLastUpdate(); }
int32_t tpw_Options_GetNumKidSprites(COptions* p) { return p->GetNumKidSprites(); }
int32_t tpw_Options_GetNumStaffSprites(COptions* p) { return p->GetNumStaffSprites(); }
void tpw_Options_balance_seed(const uint32_t* words) {
    for (int i = 0; i < 28; ++i) gOptionsBalance.mValues[i].mBits = words[i];
}
void tpw_Options_balance_read(uint32_t* words) {
    for (int i = 0; i < 28; ++i) words[i] = uint32_t(gOptionsBalance.mValues[i].mBits);
}
void* tpw_Options_create(const void* bytes) {
    auto* p = static_cast<COptions*>(::operator new(16)); new (p) COptions;
    std::memcpy(static_cast<void*>(p), bytes, 16); return p;
}
void* tpw_Options_destroy(COptions* p, int16_t flag, void* out) {
    p->~COptions(); std::memcpy(out, p, 16);
    if (flag > 0) ::operator delete(p);
    return p;
}
void tpw_Options_release(void* p) { ::operator delete(p); }
void* tpw_Options_global() { return &gOptions; }
void tpw_sinit_TPOptions() { new (&gOptions) COptions; }
}
