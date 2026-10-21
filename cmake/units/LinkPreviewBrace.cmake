# Unit v5366: the track editor's preview brace (LinkPreviewBrace, LinkPreviewBraceToPylon; src/game/rides/CoasterPylon.cpp). The callees outside the unit are
# recording fakes in tests/unit_LinkPreviewBrace.cpp, which also defines the globals of other files.
set(TPW_UNIT_LinkPreviewBrace src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_LinkPreviewBrace_KEEP
  __Z16LinkPreviewBraceP20tag_sCoasterInstance   # LinkPreviewBrace(tag_sCoasterInstance*)
  __Z23LinkPreviewBraceToPylonP20tag_sCoasterInstanceP15tag_sTrackBraceP12sPylonRecordfi   # LinkPreviewBraceToPylon(tag_sCoasterInstance*, tag_sTrackBrace*, sPylonRecord*, float, int)
  _gLinkFoundPylon
  _gLinkSavedBraces
  _gLinkSavedFlag
  _gLinkSavedHeights
  _gLinkSavedPylon
)
