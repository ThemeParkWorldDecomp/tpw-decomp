# Unit v51018: ShowStatusPanel::InitStatusWindow (src/game/ui/ui_showStatus.cpp). The callees
# outside the unit are recording fakes in tests/unit_ShowStatusPanel_InitStatusWindow.cpp.
set(TPW_UNIT_ShowStatusPanel_InitStatusWindow src/game/ui/ui_showStatus.cpp platform/arena.cpp)
set(TPW_UNIT_ShowStatusPanel_InitStatusWindow_KEEP
  __ZN15ShowStatusPanel16InitStatusWindowEv   # ShowStatusPanel::InitStatusWindow()
  __ZN17statusinit_callee10SpriteCtorEPv   # statusinit_callee::SpriteCtor(void*)
  __ZN17statusinit_callee11OperatorNewEj   # statusinit_callee::OperatorNew(unsigned int)
  __ZN17statusinit_callee12GradientCtorEPvPKc   # statusinit_callee::GradientCtor(void*, char const*)
  __ZN17statusinit_callee14WindowSetValueEP15InterfaceWindowi   # statusinit_callee::WindowSetValue(InterfaceWindow*, int)
  __ZN17statusinit_callee8CashCtorEPv   # statusinit_callee::CashCtor(void*)
  _gStatusMeshNames
)
