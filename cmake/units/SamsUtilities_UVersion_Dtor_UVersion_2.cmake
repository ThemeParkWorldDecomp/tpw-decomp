# Unit v51042: UVersion's destructor, UIMain_SetObjectStateText, InterfaceRankTextObject::Change, the lender callbacks, ThoughtMsg,
# NameEditMsg (src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_loansPanel.cpp, src/game/ui/ui_main.cpp, src/game/ui/ui_parkStatus.cpp). The callees outside the unit are
# recording fakes in tests/unit_SamsUtilities_UVersion_Dtor_UVersion_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_SamsUtilities_UVersion_Dtor_UVersion_2 src/game/ui/InterfaceTextObjectSprite.cpp
    src/game/ui/ui_Postcard.cpp src/game/ui/ui_loansPanel.cpp src/game/ui/ui_main.cpp src/game/ui/ui_parkStatus.cpp
    platform/arena.cpp)
set(TPW_UNIT_SamsUtilities_UVersion_Dtor_UVersion_2_KEEP
  __Z10ThoughtMsgP15InterfaceWindowiii   # ThoughtMsg(InterfaceWindow*, int, int, int)
  __Z11NameEditMsgP15InterfaceWindowiii   # NameEditMsg(InterfaceWindow*, int, int, int)
  __Z13UVersion_DtorPN13SamsUtilities8UVersionEs   # UVersion_Dtor(SamsUtilities::UVersion*, short)
  __Z18LenderListCallbackP15InterfaceWindowiii   # LenderListCallback(InterfaceWindow*, int, int, int)
  __Z20LendersItemsCallbackP15InterfaceWindowiii   # LendersItemsCallback(InterfaceWindow*, int, int, int)
  __Z25UIMain_SetObjectStateTextP15InterfaceWindowi   # UIMain_SetObjectStateText(InterfaceWindow*, int)
  __Z30InterfaceRankTextObject_ChangeP25InterfaceTextObjectSpritejP8DOChange   # InterfaceRankTextObject_Change(InterfaceTextObjectSprite*, unsigned int, DOChange*)
  __ZN13v51042_callee13WindowSetTextEP15InterfaceWindowPDs   # v51042_callee::WindowSetText(InterfaceWindow*, char16_t*)
  __ZN13v51042_callee14OperatorDeleteEPv   # v51042_callee::OperatorDelete(void*)
  __ZN13v51042_callee14WindowGetValueEP15InterfaceWindow   # v51042_callee::WindowGetValue(InterfaceWindow*)
  _gThoughtState
  _gUIObjectStateColours
  _gUIObjectStateStrings
)
