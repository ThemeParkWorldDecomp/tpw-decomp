# Unit v5981: InterfaceWindow::link, the island lobby controller's static initialiser, LobbyOnlineLoginPanelDlgMsg, AdressesTextPanelMsg,
# read_JPEG_file (src/game/ui/InterfaceWindow.cpp, src/game/ui/ui_islandLobbyController.cpp, src/game/ui/ui_onlineLogin.cpp, src/game/ui/ui_onlineOutbox.cpp). The callees outside the unit are recording fakes in
# tests/unit_InterfaceWindow_link.cpp, which also defines the globals of other files.
set(TPW_UNIT_InterfaceWindow_link src/game/ui/InterfaceWindow.cpp src/game/ui/ui_islandLobbyController.cpp
    src/game/ui/ui_onlineLogin.cpp src/game/ui/ui_onlineOutbox.cpp platform/arena.cpp)
set(TPW_UNIT_InterfaceWindow_link_KEEP
  __Z14read_JPEG_filePcPh   # read_JPEG_file(char*, unsigned char*)
  __Z20AdressesTextPanelMsgP15InterfaceWindowiii   # AdressesTextPanelMsg(InterfaceWindow*, int, int, int)
  __Z21read_JPEG_fileCheckedPcPh   # read_JPEG_fileChecked(char*, unsigned char*)
  __Z27LobbyOnlineLoginPanelDlgMsgP15InterfaceWindowiii   # LobbyOnlineLoginPanelDlgMsg(InterfaceWindow*, int, int, int)
  __Z36__sinit_ui_islandLobbyController_cppv   # __sinit_ui_islandLobbyController_cpp()
  __ZN13outbox_callee10JpegSetJmpEPv   # outbox_callee::JpegSetJmp(void*)
  __ZN15InterfaceWindow11linkCheckedEPS_cc   # InterfaceWindow::linkChecked(InterfaceWindow*, char, char)
  __ZN15InterfaceWindow4linkEPS_cc   # InterfaceWindow::link(InterfaceWindow*, char, char)
  _gIslandLobbyText1Node
  _gIslandLobbyText1Storage
  _gIslandLobbyText2Node
  _gIslandLobbyText2Storage
)
