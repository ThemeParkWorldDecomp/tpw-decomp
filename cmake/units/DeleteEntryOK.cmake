# Unit v51043: the postcard address delete button, two list row makers, ResearchPanelCategoryComplete and InterfaceTimedMessages::Add
# (src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_RideStatus.cpp, src/game/ui/ui_researchPanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_DeleteEntryOK.cpp, which also defines the globals of other files.
set(TPW_UNIT_DeleteEntryOK src/game/ui/InterfaceMessageQueue.cpp src/game/ui/ui_Postcard.cpp
    src/game/ui/ui_RideStatus.cpp src/game/ui/ui_researchPanel.cpp platform/arena.cpp)
set(TPW_UNIT_DeleteEntryOK_KEEP
  __Z13DeleteEntryOKPv   # DeleteEntryOK(void*)
  __Z16PostcardLCFormatv   # PostcardLCFormat()
  __Z19PostcardFontVirtualP13InterfaceFont   # PostcardFontVirtual(InterfaceFont*)
  __Z20PostcardLCPostFormatP15InterfaceWindow   # PostcardLCPostFormat(InterfaceWindow*)
  __Z20ResearchListLCFormatv   # ResearchListLCFormat()
  __Z29ResearchPanelCategoryCompletei   # ResearchPanelCategoryComplete(int)
  __ZN15postcard_callee11OperatorNewEj   # postcard_callee::OperatorNew(unsigned int)
  __ZN22InterfaceTimedMessages3AddEP15InterfaceWindowiiij   # InterfaceTimedMessages::Add(InterfaceWindow*, int, int, int, unsigned int)
  _gResearchRowHeight
)
