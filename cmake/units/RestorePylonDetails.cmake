# Unit v5667: CalculateBraceHeight, RestorePylonDetails, SquishPylonBracesIfRequired, InsertBraceIntoPylon (src/game/rides/CoasterPylon.cpp).
# The callees outside the unit are recording fakes in tests/unit_RestorePylonDetails.cpp, which also defines the globals of other files.
set(TPW_UNIT_RestorePylonDetails src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_RestorePylonDetails_KEEP
  __Z19RestorePylonDetailsP20tag_sCoasterInstanceP15sTrackBraceData   # RestorePylonDetails(tag_sCoasterInstance*, sTrackBraceData*)
  __Z20CalculateBraceHeightP15tag_sTrackBrace   # CalculateBraceHeight(tag_sTrackBrace*)
  __Z20InsertBraceIntoPylonP20tag_sCoasterInstanceP15tag_sTrackBraceP10sLinkPylonjfPfiiii   # InsertBraceIntoPylon(tag_sCoasterInstance*, tag_sTrackBrace*, sLinkPylon*, unsigned int, float, float*, int, int, int, int)
  __Z27SquishPylonBracesIfRequiredP20tag_sCoasterInstanceP13sPylonControlP10sLinkPylon   # SquishPylonBracesIfRequired(tag_sCoasterInstance*, sPylonControl*, sLinkPylon*)
)
