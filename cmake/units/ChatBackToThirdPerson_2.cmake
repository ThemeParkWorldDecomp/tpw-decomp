# Unit v5919: the online chat panel's first-person switches (ChatBackToThirdPerson, ChatGoFirstPerson, UpdateAvatarPos, ChatGoFullScreen;
# src/game/ui/ui_onlinePanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_ChatBackToThirdPerson_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_ChatBackToThirdPerson_2 src/game/ui/ui_onlinePanel.cpp platform/arena.cpp)
set(TPW_UNIT_ChatBackToThirdPerson_2_KEEP
  __Z15UpdateAvatarPosv   # UpdateAvatarPos()
  __Z16ChatGoFullScreenv   # ChatGoFullScreen()
  __Z17ChatGoFirstPersonv   # ChatGoFirstPerson()
  __Z21ChatBackToThirdPersonv   # ChatBackToThirdPerson()
  _gOnlineFirstPersonCountdown
  _gOnlineMapX
  _gOnlineMapZ
  _gOnlineSavedFlags
)
