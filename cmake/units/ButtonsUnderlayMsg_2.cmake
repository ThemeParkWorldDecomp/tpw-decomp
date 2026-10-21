# Unit v51037: the buttons menu underlay and its hide function, the online park info and list panels' static initialisers,
# RemoveOnlineParkListPanel and four UIInterface functions (src/game/ui/UIInterface.cpp, src/game/ui/ui_onlinePanel.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_onlineParkListPanel.cpp). The callees outside
# the unit are recording fakes in tests/unit_ButtonsUnderlayMsg_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_ButtonsUnderlayMsg_2 src/game/ui/UIInterface.cpp src/game/ui/ui_onlinePanel.cpp
    src/game/ui/ui_onlineParkInfoPanel.cpp src/game/ui/ui_onlineParkListPanel.cpp platform/arena.cpp)
set(TPW_UNIT_ButtonsUnderlayMsg_2_KEEP
  __Z15HideButtonsMenuv   # HideButtonsMenu()
  __Z18ButtonsUnderlayMsgP15InterfaceWindowiii   # ButtonsUnderlayMsg(InterfaceWindow*, int, int, int)
  __Z25RemoveOnlineParkListPanelv   # RemoveOnlineParkListPanel()
  __Z34__sinit_ui_onlineParkInfoPanel_cppv   # __sinit_ui_onlineParkInfoPanel_cpp()
  __Z34__sinit_ui_onlineParkListPanel_cppv   # __sinit_ui_onlineParkListPanel_cpp()
  __ZN11UIInterface10DateChangeE8ParkDate   # UIInterface::DateChange(ParkDate)
  __ZN11UIInterface10ThingDeathE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # UIInterface::ThingDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN11UIInterface14Screen_PeepLocEv   # UIInterface::Screen_PeepLoc()
  __ZN11UIInterface15Screen_StaffLocEv   # UIInterface::Screen_StaffLoc()
  _gOnlineParkInfoNodeA
  _gOnlineParkInfoNodeB
  _gOnlineParkInfoNodePark
  _gOnlineParkInfoParkDtor
  _gOnlineParkInfoStringA
  _gOnlineParkInfoStringB
  _gOnlineParkListNodeA
  _gOnlineParkListNodeB
  _gOnlineParkListStringA
  _gOnlineParkListStringB
)
