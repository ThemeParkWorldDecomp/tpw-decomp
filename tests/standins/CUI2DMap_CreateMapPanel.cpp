// The weak stand-ins of the unit library tpw_unit_CUI2DMap_CreateMapPanel
// (cmake/units/CUI2DMap_CreateMapPanel.cmake), as
// src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister_CUI2DMap_CreateMapPanel.cpp
// of before the regroup (v5L30b) defined them, unchanged. The regroup put each function into the src file of its
// original source file, where these defaults would sit next to the real definitions, so it dropped them; the unit
// library compiles them from here, so that it builds what it built before.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "CellBase.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "Light.h"
#include "TPCoaster.h"
#include "TPMain.h"
#include "TbGraphicFile.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "ui_main.h"
#include "Main.h"
#include "TbRaConvert.h"
#include "TbTList.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "tb_resource_texture.h"
#include "Advisor.h"
#include "AdvisorScoring.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "MapWho.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "tb_resource_display.h"
#include "tb_texturecache.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_tooltip.h"
#include "WindowScripts.inc"

// 0x1022bf74 (stand-in): the map panel's message function, untranslated.
__attribute__((weak)) int32_t UI_2DMap_CallbackFn(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
