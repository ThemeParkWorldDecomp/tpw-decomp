# Unit v5771: UIMenu::Show, Hide and ~UIMenu (src/game/ui/UIMenu.cpp). The callees are recording fakes in
# tests/unit_UIMenu_Show.cpp, which also defines gUIRootWindow.
set(TPW_UNIT_UIMenu_Show src/game/ui/UIMenu.cpp platform/arena.cpp)
set(TPW_UNIT_UIMenu_Show_KEEP
  __Z14UIMenuDestructP6UIMenus   # UIMenuDestruct(UIMenu*, short)
  __ZN11menu_callee14OperatorDeleteEPv   # menu_callee::OperatorDelete(void*)
  __ZN6UIMenu4HideEv   # UIMenu::Hide()
  __ZN6UIMenu4ShowEi   # UIMenu::Show(int)
)
