# Unit v5210: the online park list panel's row functions, message function and fill (src/game/ui/ui_onlineParkListPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_LCFormat.cpp, which also defines the globals of other files.
set(TPW_UNIT_LCFormat src/game/ui/ui_onlineParkListPanel.cpp platform/arena.cpp)
set(TPW_UNIT_LCFormat_KEEP
  __Z12FillParkListP9TbPtrListI13CFSOnlineParkE   # FillParkList(TbPtrList<CFSOnlinePark>*)
  __Z22OnlineParkListLCFormatv   # OnlineParkListLCFormat()
  __Z22OnlineParkListPanelMsgP15InterfaceWindowiii   # OnlineParkListPanelMsg(InterfaceWindow*, int, int, int)
  __Z26OnlineParkListLCPostFormatP15InterfaceWindow   # OnlineParkListLCPostFormat(InterfaceWindow*)
  __ZN15parklist_callee9FilterMslEP11SwearFilterPvb   # parklist_callee::FilterMsl(SwearFilter*, void*, bool)
  _gOnlineParkListRowHeight
  _gOnlineParkListRowInit
)
