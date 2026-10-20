// tb_lightmanager.h: the light manager's two working globals (engine data, not exported; the
// names are invented). They sit between gLightInverseWorld (0x100af150, tb_light.h) and
// GlobalDiffuse (0x100af1a8, MapWho.h) and are defined in src/tb_lightmanager_frame.cpp.
#pragma once

class CMatrix;

// The world matrix CLightManager::PrepareLighting was given last (a pointer to the caller's
// matrix, not a copy).
extern CMatrix* gLightWorld;      // 0x100af190

// What CLightManager::CalcDiffuseColor works out: alpha, red, green and blue. It starts as a
// copy of GlobalDiffuse and each light adds its share.
extern float gDiffuseResult[4];   // 0x100af198
