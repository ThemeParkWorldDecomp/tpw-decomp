# Unit v51008: GenerateTrackSubdivisions, CoasterSetSpeed, GetBankAtBrace, FlashEndPylons, DecCoasterConst1 (src/game/rides/CoasterExtrude.cpp, src/game/rides/CoasterTrack.cpp, src/game/rides/CoasterTrain.cpp).
# The callees outside the unit are recording fakes in tests/unit_GenerateTrackSubdivisions_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_GenerateTrackSubdivisions_2 src/game/rides/CoasterExtrude.cpp src/game/rides/CoasterTrack.cpp
    src/game/rides/CoasterTrain.cpp platform/arena.cpp)
set(TPW_UNIT_GenerateTrackSubdivisions_2_KEEP
  __Z14FlashEndPylonsP20tag_sCoasterInstancej   # FlashEndPylons(tag_sCoasterInstance*, unsigned int)
  __Z14GetBankAtBraceP20tag_sCoasterInstanceP15tag_sTrackBracef   # GetBankAtBrace(tag_sCoasterInstance*, tag_sTrackBrace*, float)
  __Z15CoasterSetSpeedP20tag_sCoasterInstanceji   # CoasterSetSpeed(tag_sCoasterInstance*, unsigned int, int)
  __Z16DecCoasterConst1v   # DecCoasterConst1()
  __Z25GenerateTrackSubdivisionsP15tag_sTrackBraceS0_P16sExtrusionParamsPff   # GenerateTrackSubdivisions(tag_sTrackBrace*, tag_sTrackBrace*, sExtrusionParams*, float*, float)
)
