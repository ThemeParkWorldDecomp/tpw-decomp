# Unit v5572: CreateParkStatusPanel (src/game/ui/ui_parkStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_CreateParkStatusPanel.cpp, which also defines the globals of other files.
set(TPW_UNIT_CreateParkStatusPanel src/game/ui/ui_parkStatus.cpp platform/arena.cpp
    tests/standins/CreateParkStatusPanel.cpp)
set(TPW_UNIT_CreateParkStatusPanel_KEEP
  __Z21CreateParkStatusPanelv   # CreateParkStatusPanel()
  __ZN17parkstatus_callee10NumberCtorEPv   # parkstatus_callee::NumberCtor(void*)
  __ZN17parkstatus_callee10SpriteCtorEPv   # parkstatus_callee::SpriteCtor(void*)
  __ZN17parkstatus_callee11OperatorNewEj   # parkstatus_callee::OperatorNew(unsigned int)
  __ZN17parkstatus_callee12GradientCtorEPvPKc   # parkstatus_callee::GradientCtor(void*, char const*)
  __ZN17parkstatus_callee13WindowSetTextEP15InterfaceWindowPKDs   # parkstatus_callee::WindowSetText(InterfaceWindow*, char16_t const*)
  __ZN17parkstatus_callee14DrawSpriteCtorEPv   # parkstatus_callee::DrawSpriteCtor(void*)
  __ZN17parkstatus_callee14WindowSetValueEP15InterfaceWindowi   # parkstatus_callee::WindowSetValue(InterfaceWindow*, int)
  _gParkStatusMeshNames
  _gParkStatusScript
)
