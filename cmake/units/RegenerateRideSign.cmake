# Unit v5934: RegenerateRideSign, RegenerateRideSigns, SetRideScriptID, GetAnimStats and the object instance table (src/game/rides/RidePlacement.cpp).
# The callees outside the unit are recording fakes in tests/unit_RegenerateRideSign.cpp, which also defines the globals of other files.
set(TPW_UNIT_RegenerateRideSign src/game/rides/RidePlacement.cpp platform/arena.cpp)
set(TPW_UNIT_RegenerateRideSign_KEEP
  __Z12GetAnimStatsP16tag_sRideAnimSetjPjS1_S1_   # GetAnimStats(tag_sRideAnimSet*, unsigned int, unsigned int*, unsigned int*, unsigned int*)
  __Z15SetRideScriptIDjj   # SetRideScriptID(unsigned int, unsigned int)
  __Z16GetAnimStatsNameP16tag_sRideAnimSetjPjS1_S1_   # GetAnimStatsName(tag_sRideAnimSet*, unsigned int, unsigned int*, unsigned int*, unsigned int*)
  __Z18RegenerateRideSignPvPKDsS1_   # RegenerateRideSign(void*, char16_t const*, char16_t const*)
  __Z19RegenerateRideSignsv   # RegenerateRideSigns()
  __Z24RegenerateRideSignResultPvPKDsS1_   # RegenerateRideSignResult(void*, char16_t const*, char16_t const*)
  __Z29AllocateObjectInstanceToTableRjS_S_jPPvS0_j   # AllocateObjectInstanceToTable(unsigned int&, unsigned int&, unsigned int&, unsigned int, void**, void*, unsigned int)
  __Z29RemoveObjectInstanceFromTableRjS_S_PPvj   # RemoveObjectInstanceFromTable(unsigned int&, unsigned int&, unsigned int&, void**, unsigned int)
  _gRideSignListEnd
)
