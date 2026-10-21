# Unit v5625: GenerateHoardingMeshes (src/game/park/TPHoardings.cpp). The callees outside the unit are recording fakes in
# tests/unit_GenerateHoardingMeshes.cpp, which also defines gRideHoardingTextures. List only the sources the library must compile.
set(TPW_UNIT_GenerateHoardingMeshes src/game/park/TPHoardings.cpp platform/arena.cpp)
set(TPW_UNIT_GenerateHoardingMeshes_KEEP
  __Z22GenerateHoardingMeshesPKcP16tag_sRideAnimSetPv   # GenerateHoardingMeshes(char const*, tag_sRideAnimSet*, void*)
  _gHoardingSortOrder
  _gHoardingSortScratch
  _gHoardingSortSource
  _gHoardingTexturePathFormat
)
