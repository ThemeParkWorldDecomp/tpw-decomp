// A callee of the unit library tpw_unit_LayoutSetMovePylon (cmake/units/LayoutSetMovePylon.cmake) that the regroup
// (v5L30b) changed. src/game/rides/CoasterEdit.cpp now holds both overloads of SetCoasterEdit above
// LayoutSetCoasterEditOld, so its call with a tag_sCoasterInstance* picks SetCoasterEdit(tag_sCoasterInstance*, int)
// (0x100585f4, the callee the Mac code calls) instead of SetCoasterEdit(void*, int). The test library fakes the
// latter only and its PowerPC side stubs 0x100585f4, so this hands the call to that fake with the same arguments.

#include <cstdint>

#include "TPCoaster.h"

void SetCoasterEdit(void* coaster, int32_t flag);

void SetCoasterEdit(tag_sCoasterInstance* instance, int32_t flag) { SetCoasterEdit(static_cast<void*>(instance), flag); }
