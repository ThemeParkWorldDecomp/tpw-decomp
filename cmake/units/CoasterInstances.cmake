# Unit w186: the coaster instances (src/game/rides/CoasterPylon.cpp, src/game/rides/CoasterType.cpp, src/game/rides/TPCoaster.cpp) and ~CCoasterBalance (src/game/rides/CoasterType.cpp, src/game/rides/TPCoaster.cpp). The callees outside the unit are
# recording fakes in tests/unit_CoasterInstances.cpp, which also defines the globals of other files.
set(TPW_UNIT_CoasterInstances src/game/rides/CoasterPylon.cpp src/game/rides/CoasterType.cpp
    src/game/rides/TPCoaster.cpp platform/arena.cpp)
set(TPW_UNIT_CoasterInstances_KEEP
  __Z13IsCoasterTypej   # IsCoasterType(unsigned int)
  __Z15InstanceCoasterjjjjiPv   # InstanceCoaster(unsigned int, unsigned int, unsigned int, unsigned int, int, void*)
  __Z15SetCoasterStateP20tag_sCoasterInstancej   # SetCoasterState(tag_sCoasterInstance*, unsigned int)
  __Z16DestroyEndPylonsP20tag_sCoasterInstance   # DestroyEndPylons(tag_sCoasterInstance*)
  __Z17FlagGenerateTrackP20tag_sCoasterInstancei   # FlagGenerateTrack(tag_sCoasterInstance*, int)
  __Z17SetCoasterCObjectjPv   # SetCoasterCObject(unsigned int, void*)
  __Z18SetCoasterScriptIDji   # SetCoasterScriptID(unsigned int, int)
  __Z20GetHeightForIntervalfP13sPylonControl   # GetHeightForInterval(float, sPylonControl*)
  __Z20GetIntervalForHeightfP13sPylonControl   # GetIntervalForHeight(float, sPylonControl*)
  __Z20XZAngleBetweenPylonsjjjjjj   # XZAngleBetweenPylons(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int)
  __Z21SetCurrentEditCoasterP20tag_sCoasterInstance   # SetCurrentEditCoaster(tag_sCoasterInstance*)
  __Z22ui_GetNumCoasterTracksv   # ui_GetNumCoasterTracks()
  __Z25GetIntervalForHeightDeltafP13sPylonControl   # GetIntervalForHeightDelta(float, sPylonControl*)
  __Z29GetPotentialPylonPositionAreaPjS_S_S_   # GetPotentialPylonPositionArea(unsigned int*, unsigned int*, unsigned int*, unsigned int*)
  __Z33FindCoasterInstanceFromEntranceIDj   # FindCoasterInstanceFromEntranceID(unsigned int)
  __Z35FlagCollisionUpdateTrackAroundBraceP15tag_sTrackBrace   # FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace*)
  __Z8EndTracki   # EndTrack(int)
  __Z9IsCoasterj   # IsCoaster(unsigned int)
  __ZN15CCoasterBalance21GetBalanceDescriptionEi   # CCoasterBalance::GetBalanceDescription(int)
  __ZN15CCoasterBalance21GetBalanceValuesStartEv   # CCoasterBalance::GetBalanceValuesStart()
  __ZN15CCoasterBalanceD1Ev   # CCoasterBalance::~CCoasterBalance()
  _gCoasterBalanceDescriptions
  _gCoasterInstanceCount
  _gCoasterInstances
  _gCoasterWord6890
  _gCoasterWord6894
  _gCoasterWord6898
  _gCoasterWord689C
  TRAINS_SWAP_FILE_WORDS@game/rides/TPCoaster.cpp   # the #define TRAINS_SWAP_FILE_WORDS of src/game/rides/TPCoaster.cpp (a guard macro the regroup brought into it)
)
