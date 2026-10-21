# w67: the lightning midpoint, subdivision and textured quad builder.
set(TPW_UNIT_RainBolt src/engine/Rain.cpp)
set(TPW_UNIT_RainBolt_KEEP
  __Z13GeneratePointP10_D3DVECTORS0_S0_f   # GeneratePoint(_D3DVECTOR*, _D3DVECTOR*, _D3DVECTOR*, float)
  __ZN10SLightning4InitE10_D3DVECTORS0_ii   # SLightning::Init(_D3DVECTOR, _D3DVECTOR, int, int)
  __ZN5CRain12AddLightningE10_D3DVECTORS0_ii   # CRain::AddLightning(_D3DVECTOR, _D3DVECTOR, int, int)
  __ZN5CRain18NewRenderLightningEP10_D3DVECTORS1_   # CRain::NewRenderLightning(_D3DVECTOR*, _D3DVECTOR*)
  __ZN5CRain19AddLightningSectionEP10_D3DVECTORS1_   # CRain::AddLightningSection(_D3DVECTOR*, _D3DVECTOR*)
)
# A realistic bolt needs a 2,000-word Random queue.
set(TPW_UNIT_RainBolt_OWN_CALLLOG ON)
# Only the test library counts entries to the real recursive function.
cmake_language(DEFER CALL target_compile_options tpw_unit_RainBolt PRIVATE -finstrument-functions)
