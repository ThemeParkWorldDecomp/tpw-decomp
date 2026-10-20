// tb_light.h: tb_light.cpp holds only a global matrix (engine library; its static
// initialiser at 0x10014200 is the whole file's code).
#pragma once
#include "tb_matrix.h"

// Identity after static initialisation. CLightManager::PrepareLighting (the file after
// tb_light.cpp) copies an object's world matrix here and inverts it to bring the lights
// into object space. Engine data 0x100af150, not exported; name invented.
extern CMatrix gLightInverseWorld;
