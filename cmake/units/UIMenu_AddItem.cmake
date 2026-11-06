# Unit v5163: UIMenu::Init and UIMenu::AddItem (src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/UIMenu.cpp, src/game/ui/UI_Base.cpp). The callees outside the unit are recording fakes in
# tests/unit_UIMenu_AddItem.cpp, which also replaces the weak menu_callee functions of the src file.
set(TPW_UNIT_UIMenu_AddItem src/game/ui/InterfaceTextObjectSprite.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/UIMenu.cpp src/game/ui/UI_Base.cpp src/game/ui/InterfaceStringHeap.cpp platform/arena.cpp)
set(TPW_UNIT_UIMenu_AddItem_KEEP
  __Z19UIMenuHandlerHandlePFiP15InterfaceWindowiiiE   # UIMenuHandlerHandle(int (*)(InterfaceWindow*, int, int, int))
  __Z23UIMenuHandlerFromHandlej   # UIMenuHandlerFromHandle(unsigned int)
  __ZN11menu_callee13TitleTextCtorEPv   # menu_callee::TitleTextCtor(void*)
  __ZN11menu_callee13WindowSetTextEP15InterfaceWindowPDs   # menu_callee::WindowSetText(InterfaceWindow*, char16_t*)
  __ZN11menu_callee8RectCtorEPv   # menu_callee::RectCtor(void*)
  __ZN11menu_callee8RectDtorEPv   # menu_callee::RectDtor(void*)
  __ZN6UIMenu4InitEPFiP15InterfaceWindowiiiES3_   # UIMenu::Init(int (*)(InterfaceWindow*, int, int, int), int (*)(InterfaceWindow*, int, int, int))
  __ZN6UIMenu7AddItemEiPKDsiiiii   # UIMenu::AddItem(int, char16_t const*, int, int, int, int, int)
  __ZN19InterfaceStringHeap10GetAndCopyEPKDs   # InterfaceStringHeap::GetAndCopy(char16_t const*)
  __ZN19InterfaceStringHeap13ReleaseStringEPDs   # InterfaceStringHeap::ReleaseString(char16_t*)
  __ZN19InterfaceStringHeap9GetStringEi   # InterfaceStringHeap::GetString(int)
  __Z26UIMenu_DefaultItemCallbackP15InterfaceWindowiii   # UIMenu_DefaultItemCallback(InterfaceWindow*, int, int, int)
  _gUIUnitHeight
  __Z13UI_CentreDownPK15InterfaceWindow   # UI_CentreDown(InterfaceWindow const*)
)
