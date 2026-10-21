# Unit v5053: GetNewBraceLoftInterval, DetermineLinkLevel (src/game/rides/CoasterPylon.cpp). The callees outside the unit are recording fakes in
# tests/unit_GetNewBraceLoftInterval.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetNewBraceLoftInterval src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_GetNewBraceLoftInterval_KEEP
  __Z18DetermineLinkLevelfP10sLinkPylonPjPfi   # DetermineLinkLevel(float, sLinkPylon*, unsigned int*, float*, int)
  __Z23GetNewBraceLoftIntervalP10sLinkPylonP13sPylonControlP15tag_sTrackBraceS4_iiPf   # GetNewBraceLoftInterval(sLinkPylon*, sPylonControl*, tag_sTrackBrace*, tag_sTrackBrace*, int, int, float*)
)
