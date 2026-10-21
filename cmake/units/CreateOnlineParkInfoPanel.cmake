# Unit v5537: CreateOnlineParkInfoPanel (src/game/ui/ui_onlineParkInfoPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateOnlineParkInfoPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateOnlineParkInfoPanel src/game/ui/ui_onlineParkInfoPanel.cpp platform/arena.cpp)
set(TPW_UNIT_CreateOnlineParkInfoPanel_KEEP
  __Z25CreateOnlineParkInfoPanelPK13CFSOnlineParkP15InterfaceWindowPFvvEPFviPKvEPv   # CreateOnlineParkInfoPanel(CFSOnlinePark const*, InterfaceWindow*, void (*)(), void (*)(int, void const*), void*)
  _gOnlineParkInfoCancel
  _gOnlineParkInfoContext
  _gOnlineParkInfoDownload
  _gOnlineParkInfoPark
)
