# Unit v5272: light-node helpers (src/game/TPLevel.cpp). UpdateLights is a recording fake in tests/unit_SetLightColourExplicit.cpp.
set(TPW_UNIT_SetLightColourExplicit src/game/TPLevel.cpp platform/arena.cpp)
set(TPW_UNIT_SetLightColourExplicit_KEEP
  __Z11EnableLightP16sNodeLightParams   # EnableLight(sNodeLightParams*)
  __Z12DisableLightP16sNodeLightParams   # DisableLight(sNodeLightParams*)
  __Z22SetLightColourExplicitP16sNodeLightParamsfff   # SetLightColourExplicit(sNodeLightParams*, float, float, float)
  __Z22SetLightColourRelativeP16sNodeLightParamsf   # SetLightColourRelative(sNodeLightParams*, float)
)
