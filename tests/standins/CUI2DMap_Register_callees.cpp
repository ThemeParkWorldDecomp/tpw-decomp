// A callee of the unit library tpw_unit_CUI2DMap_Register (cmake/units/CUI2DMap_Register.cmake) that the regroup
// (v5L30b) made external. src/game/ui/ui_2dmap.cpp declares the explicit specialization
// TbTList<CUI2DMapSprite*, CUI2DMapSprite*>::AddTail(CUI2DMapSprite*) for the sprite constructor, and since the
// regroup the weak ui2dmap_callee::ListAddTail sits below that declaration, so it calls the function instead of
// inlining the template. The test replaces ListAddTail but its weak copy still names the function, and no file
// of src/ defines that specialization (the program links the template's own instance in src/v5_twins.cpp), so
// this instantiates the template here, as the copy before the regroup did inline.

#include "TbTList.h"
#include "ui_main.h"

template void TbTList<CUI2DMapSprite*, CUI2DMapSprite*>::AddTail(CUI2DMapSprite*);
