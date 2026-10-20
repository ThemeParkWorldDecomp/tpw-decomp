#include <new>
#include "VanillaTime.h"
extern "C" {
bool tpw_VanillaTime_Save(CVanillaTime* p, void* file) { return p->Save(file); }
bool tpw_VanillaTime_Load(CVanillaTime* p, void* file) { return p->Load(file); }
CVanillaTime* tpw_VanillaTime_global() { return &gVanillaTime; }
void tpw_sinit_VanillaTime() { new (&gVanillaTime) CVanillaTime(); }
}
