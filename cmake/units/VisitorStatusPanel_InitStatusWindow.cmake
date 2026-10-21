# Unit v5521: VisitorStatusPanel::InitStatusWindow (src/game/ui/InterfaceTextObjectSprite.cpp, src/game/ui/ui_visitorStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_VisitorStatusPanel_InitStatusWindow.cpp.
set(TPW_UNIT_VisitorStatusPanel_InitStatusWindow src/game/ui/InterfaceTextObjectSprite.cpp
    src/game/ui/ui_visitorStatus.cpp platform/arena.cpp)
set(TPW_UNIT_VisitorStatusPanel_InitStatusWindow_KEEP
  __ZN10vsp_callee12GradientCtorEPvS0_   # vsp_callee::GradientCtor(void*, void*)
  __ZN10vsp_callee8TimeCtorEPv   # vsp_callee::TimeCtor(void*)
  __ZN18VisitorStatusPanel16InitStatusWindowEv   # VisitorStatusPanel::InitStatusWindow()
)
