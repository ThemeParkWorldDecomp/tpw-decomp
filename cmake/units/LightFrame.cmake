set(TPW_UNIT_LightFrame src/engine/LightManager.cpp src/engine/MapWho.cpp src/engine/RenderMemory.cpp
    platform/arena.cpp)
set(TPW_UNIT_LightFrame_KEEP
  __ZN12SMapLinkType13PrepareLightsEv   # SMapLinkType::PrepareLights()
  __ZN13CLightManager15CalcIntensitiesEPjP10_D3DVECTORP7SVertexi   # CLightManager::CalcIntensities(unsigned int*, _D3DVECTOR*, SVertex*, int)
  __ZN13CLightManager15PrepareLightingEP7CMatrixP11sAlignedBox   # CLightManager::PrepareLighting(CMatrix*, sAlignedBox*)
  __ZN13CLightManager16CalcDiffuseColorEP10_D3DVECTORfff   # CLightManager::CalcDiffuseColor(_D3DVECTOR*, float, float, float)
  __ZN13CLightManager20CalcNodeVertexColorsEPjP9tag_sNode   # CLightManager::CalcNodeVertexColors(unsigned int*, tag_sNode*)
  __ZN13CLightManager24CalcVertexFogIntensitiesEP11SFVF_TLCTEXii   # CLightManager::CalcVertexFogIntensities(SFVF_TLCTEX*, int, int)
  __ZN13CLightManager9AddLightsEPP6CLight   # CLightManager::AddLights(CLight**)
  __ZN13CRenderMemory19GetIndexEndPositionEv   # CRenderMemory::GetIndexEndPosition()
  __ZN13CRenderMemory20GetVertexEndPositionEv   # CRenderMemory::GetVertexEndPosition()
  __ZN13CRenderMemory8SaveInfoEPcPt   # CRenderMemory::SaveInfo(char*, unsigned short*)
  _gDiffuseResult
  _gLightWorld
)
