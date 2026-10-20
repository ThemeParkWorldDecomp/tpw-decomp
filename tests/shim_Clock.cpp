// extern "C" wrappers for main app Clock.cpp, 0x101472f4..0x10147754.
#include <new>
#include "Clock.h"

extern "C" {
int32_t tpw_CClock_Read(CClock* c) { return c->Read(); }
int32_t tpw_CClock_ReadFixed(CClock* c) { return c->ReadFixed(); }
void tpw_CClock_Pause(CClock* c) { c->Pause(); }
void tpw_CClock_UnPause(CClock* c) { c->UnPause(); }
void tpw_CClock_TogglePause(CClock* c) { c->TogglePause(); }
int32_t tpw_CClock_GetPauseFlag(CClock* c) { return c->GetPauseFlag(); }
int32_t tpw_CClock_Save(CClock* c, void* file) { return c->Save(file); }
int32_t tpw_CClock_Load(CClock* c, void* file) { return c->Load(file); }
void tpw_CClock_PreSave(CClock* c) { c->PreSave(); }
void tpw_CClock_PostLoad(CClock* c) { c->PostLoad(); }
void tpw_sinit_Clock() { new (&gClock) CClock; }
CClock* tpw_gClock() { return &gClock; }
}
