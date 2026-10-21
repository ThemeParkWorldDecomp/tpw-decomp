set(TPW_UNIT_RideAnimLoad src/game/rides/TPRide.cpp src/game/park/TPHField.cpp platform/arena.cpp)
set(TPW_UNIT_RideAnimLoad_KEEP
  __Z14SaveHFieldCopyP15tag_sM3D2HeaderPP7sHField   # SaveHFieldCopy(tag_sM3D2Header*, sHField**)
  __Z15LoadRideAnimSetPcjPP16tag_sRideAnimSetS_j   # LoadRideAnimSet(char*, unsigned int, tag_sRideAnimSet**, char*, unsigned int)
  __Z20LoadLevelHeightfieldii   # LoadLevelHeightfield(int, int)
  __Z23DestroyLevelHeightfieldv   # DestroyLevelHeightfield()
  _gHeightfieldCopy
  _gHeightfieldHeader
  _gHeightfieldQuads
  _gHeightfieldRideSet
)
