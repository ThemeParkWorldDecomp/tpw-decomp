// Other engine files' startup entry points, used by tb_init.cpp. PARTIAL.
#pragma once
#include <cstdint>
#include "tb_drawprimitive.h"
#include "MapWho.h"                // MapWho_Init/Close, LightManager_Init/Close
#include "Wavelet.h"               // CreateSineTable
#include "tb_hfield.h"             // Gamut_Init/Close, HField_Init/Close
#include "tb_matrix.h"             // Matrix_Init, Matrix_Close
#include "tb_particle.h"           // Particle_Init/Close
#include "tb_rendermemory.h"       // RenderMemory_Init/Close
#include "tb_resource_display.h"   // RenderList_Init/Close
#include "tb_starfield.h"          // SpriteManager_Init/Close
#include "tb_switch.h"             // Switch_Init/Close
int32_t TCache_Init(int32_t format);
int32_t TCache_Close();
int32_t Rain_Init();
int32_t Rain_Close();

