# Unit v5190: SetPotentialPylonArea and PlacePylon (src/game/rides/CoasterPylon.cpp). The callees outside the unit are recording fakes in
# tests/unit_SetPotentialPylonArea.cpp, which also defines the globals of other files.
set(TPW_UNIT_SetPotentialPylonArea src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_SetPotentialPylonArea_KEEP
  __Z10PlacePylonjjiPP15sTrackBraceDataPP15tag_sTrackBracehj   # PlacePylon(unsigned int, unsigned int, int, sTrackBraceData**, tag_sTrackBrace**, unsigned char, unsigned int)
  __Z21SetPotentialPylonAreav   # SetPotentialPylonArea()
)
