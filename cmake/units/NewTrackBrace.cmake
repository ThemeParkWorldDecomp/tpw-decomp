# Unit v5157: NewTrackBrace and UnLinkPreviewBrace (src/game/rides/CoasterPylon.cpp). The callees outside the unit are recording fakes in
# tests/unit_NewTrackBrace.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_NewTrackBrace src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_NewTrackBrace_KEEP
  __Z13NewTrackBraceP12sPylonRecordfffjP13sPylonControljjj   # NewTrackBrace(sPylonRecord*, float, float, float, unsigned int, sPylonControl*, unsigned int, unsigned int, unsigned int)
  __Z18UnLinkPreviewBraceP20tag_sCoasterInstance   # UnLinkPreviewBrace(tag_sCoasterInstance*)
)
