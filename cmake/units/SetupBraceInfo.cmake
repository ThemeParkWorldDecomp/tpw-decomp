# Unit v5731: SetupBraceInfo, DestroyBraceInfo, LinkPylon, LinkBrace, UnLinkBrace (src/game/rides/CoasterPylon.cpp). The callees outside
# the unit are recording fakes in tests/unit_SetupBraceInfo.cpp, which also defines the globals of other files.
set(TPW_UNIT_SetupBraceInfo src/game/rides/CoasterPylon.cpp platform/arena.cpp)
set(TPW_UNIT_SetupBraceInfo_KEEP
  __Z11UnLinkBraceP15tag_sTrackBraceP19sCoasterTrackParams   # UnLinkBrace(tag_sTrackBrace*, sCoasterTrackParams*)
  __Z14SetupBraceInfoP15tag_sTrackBraceP10sBraceInfoP13sBraceControlP13sPylonControl   # SetupBraceInfo(tag_sTrackBrace*, sBraceInfo*, sBraceControl*, sPylonControl*)
  __Z16DestroyBraceInfoP15tag_sTrackBraceP10sBraceInfo   # DestroyBraceInfo(tag_sTrackBrace*, sBraceInfo*)
  __Z9LinkBraceP15tag_sTrackBraceP19sCoasterTrackParamsi   # LinkBrace(tag_sTrackBrace*, sCoasterTrackParams*, int)
  __Z9LinkPylonP12sPylonRecordP19sCoasterTrackParamsjjj   # LinkPylon(sPylonRecord*, sCoasterTrackParams*, unsigned int, unsigned int, unsigned int)
)
