# Unit v5557: GenerateExtrusions (src/game/rides/CoasterExtrude.cpp). The callees outside the unit are recording fakes in
# tests/unit_GenerateExtrusions.cpp, which also defines the globals of other files.
set(TPW_UNIT_GenerateExtrusions src/game/rides/CoasterExtrude.cpp platform/arena.cpp)
set(TPW_UNIT_GenerateExtrusions_KEEP
  __Z18GenerateExtrusionsv   # GenerateExtrusions()
  _gExtrudeBlendMax
  _gExtrudeBlendMin
  _gExtrudeParamsBuf
)
