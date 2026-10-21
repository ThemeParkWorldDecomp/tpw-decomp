# Unit v5193: UpdateSatellite and UI2DMReturnScaleColour (src/game/ui/ui_2dmap.cpp, src/game/ui/ui_worldLobbyController.cpp). The callees outside the unit are recording fakes in
# tests/unit_UpdateSatellite.cpp.
set(TPW_UNIT_UpdateSatellite src/game/ui/ui_2dmap.cpp src/game/ui/ui_worldLobbyController.cpp platform/arena.cpp)
set(TPW_UNIT_UpdateSatellite_KEEP
  __Z15UpdateSatelliteii   # UpdateSatellite(int, int)
  __Z22UI2DMReturnScaleColouriRKN3bfl4draw7CColourES3_S3_   # UI2DMReturnScaleColour(int, bfl::draw::CColour const&, bfl::draw::CColour const&, bfl::draw::CColour const&)
  _gUISatelliteParent
)
