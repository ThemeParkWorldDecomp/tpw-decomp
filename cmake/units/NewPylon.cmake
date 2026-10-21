# Unit v5091: NewPylon and UpdatePylonStackMeshes (src/game/rides/CoasterPylon.cpp). The callees outside the unit are recording fakes in
# tests/unit_NewPylon.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_NewPylon src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_NewPylon_KEEP
  __Z22UpdatePylonStackMeshesP13sPylonControlP15sTrackBraceDataii   # UpdatePylonStackMeshes(sPylonControl*, sTrackBraceData*, int, int)
  __Z8NewPylonjjjiP13sPylonControlP8s3DPoint   # NewPylon(unsigned int, unsigned int, unsigned int, int, sPylonControl*, s3DPoint*)
  _gPylonStackSkip
)
