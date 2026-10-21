# Unit v5L21: FindDataNodeIndexByDisableCode (src/game/TPEmbed.cpp, src/game/TPMesh.cpp, src/game/rides/RideAnim.cpp) and CTRACE (src/game/Debug.cpp).
set(TPW_UNIT_FindDataNodeIndexByDisableCode src/game/TPEmbed.cpp src/game/TPMesh.cpp src/game/rides/RideAnim.cpp
    src/game/Debug.cpp platform/arena.cpp)
set(TPW_UNIT_FindDataNodeIndexByDisableCode_KEEP
  __Z11GetUniqueID8eIDClassj   # GetUniqueID(eIDClass, unsigned int)
  __Z13GetAnimLengthP16tag_sRideAnimSetjj   # GetAnimLength(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z15GetDeferredAnimP16tag_sRideAnimSetPjS1_j   # GetDeferredAnim(tag_sRideAnimSet*, unsigned int*, unsigned int*, unsigned int)
  __Z16SetMeshInUISpaceRP9tag_sMesh   # SetMeshInUISpace(tag_sMesh*&)
  __Z21FindDataNodeIndexByIDP16tag_sRideAnimSetjj   # FindDataNodeIndexByID(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z30FindDataNodeIndexByDisableCodeP16tag_sRideAnimSetjj   # FindDataNodeIndexByDisableCode(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z36FindDataNodeIndexByIDAdvisorTempHackP16tag_sRideAnimSetjj   # FindDataNodeIndexByIDAdvisorTempHack(tag_sRideAnimSet*, unsigned int, unsigned int)
  __Z5TRACEPKcz   # TRACE(char const*, ...)
  __Z6CTRACEjPKcz   # CTRACE(unsigned int, char const*, ...)
)
