# Unit v5163: UIMenu::Init and UIMenu::AddItem (src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/UIMenu.cpp, src/game/ui/UI_Base.cpp). The callees outside the unit are recording fakes in
# tests/unit_UIMenu_AddItem.cpp, which also replaces the weak menu_callee functions of the src file.
set(TPW_UNIT_UIMenu_AddItem src/game/ui/InterfaceTextObjectSprite.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/UIMenu.cpp src/game/ui/UI_Base.cpp platform/arena.cpp)
set(TPW_UNIT_UIMenu_AddItem_KEEP
  __Z19UIMenuHandlerHandlePFiP15InterfaceWindowiiiE   # UIMenuHandlerHandle(int (*)(InterfaceWindow*, int, int, int))
  __Z23UIMenuHandlerFromHandlej   # UIMenuHandlerFromHandle(unsigned int)
  __ZN11menu_callee10GetAndCopyEPKDs   # menu_callee::GetAndCopy(char16_t const*)
  __ZN11menu_callee13TitleTextCtorEPv   # menu_callee::TitleTextCtor(void*)
  __ZN11menu_callee13WindowSetTextEP15InterfaceWindowPDs   # menu_callee::WindowSetText(InterfaceWindow*, char16_t*)
  __ZN11menu_callee4RandEv   # menu_callee::Rand()
  __ZN11menu_callee8RectCtorEPv   # menu_callee::RectCtor(void*)
  __ZN11menu_callee8RectDtorEPv   # menu_callee::RectDtor(void*)
  __ZN6UIMenu4InitEPFiP15InterfaceWindowiiiES3_   # UIMenu::Init(int (*)(InterfaceWindow*, int, int, int), int (*)(InterfaceWindow*, int, int, int))
  __ZN6UIMenu7AddItemEiPKDsiiiii   # UIMenu::AddItem(int, char16_t const*, int, int, int, int, int)
)
