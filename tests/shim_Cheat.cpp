// extern "C" wrappers for CCheat, main app 0x10146f24..0x10147294.
#include <new>
#include "Cheat.h"
#include "CMessageCentre.h"

extern "C" {
void tpw_CCheat_ctor(CCheat* c) { new (c) CCheat; }
void tpw_CCheat_SetCheatStatus(CCheat* c, bool status) { c->SetCheatStatus(status); }
uint8_t tpw_CCheat_IsCheating(CCheat* c) { return c->IsCheating(); }
int32_t tpw_CCheat_Load(CCheat* c, void** file) { return c->Load(*file); }
int32_t tpw_CCheat_Save(const CCheat* c, void** file) { return c->Save(*file); }
void tpw_cheat_set_centre(CMessageCentre* centre) { gMessageCentre = centre; }
CMessageCentre* tpw_cheat_get_centre() { return gMessageCentre; }
void tpw_cheat_release_centre() { delete gMessageCentre; gMessageCentre = nullptr; }
}
