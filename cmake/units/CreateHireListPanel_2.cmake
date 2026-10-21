# Unit v51019: CreateHireListPanel (src/game/ui/ui_hirePanel.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateHireListPanel_2.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CreateHireListPanel_2 src/game/ui/ui_hirePanel.cpp platform/arena.cpp
    tests/standins/CreateHireListPanel_2.cpp)
set(TPW_UNIT_CreateHireListPanel_2_KEEP
  __Z19CreateHireListPanel9StaffType   # CreateHireListPanel(StaffType)
  __ZN15hirelist_callee10SpriteCtorEPv   # hirelist_callee::SpriteCtor(void*)
  __ZN15hirelist_callee11OperatorNewEj   # hirelist_callee::OperatorNew(unsigned int)
  __ZN15hirelist_callee12GradientCtorEPvPKc   # hirelist_callee::GradientCtor(void*, char const*)
  __ZN15hirelist_callee13WindowSetTextEP15InterfaceWindowPDs   # hirelist_callee::WindowSetText(InterfaceWindow*, char16_t*)
  __ZN15hirelist_callee14WindowSetValueEP15InterfaceWindowi   # hirelist_callee::WindowSetValue(InterfaceWindow*, int)
  __ZN15hirelist_callee4LabsEi   # hirelist_callee::Labs(int)
  __ZN15hirelist_callee8CashCtorEPv   # hirelist_callee::CashCtor(void*)
  __ZN15hirelist_callee9TitleCtorEPv   # hirelist_callee::TitleCtor(void*)
  _gHireColumnColour
  _gHireListDeposits
  _gHireListStrings
  _gHireListWithdrawals
  _gHireListWord10
  _gHireListWord14
  _gHirePanelScript
  _gHireSortColumn
)
