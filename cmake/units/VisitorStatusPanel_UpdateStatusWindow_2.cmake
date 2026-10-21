# Unit v51022: VisitorStatusPanel::UpdateStatusWindow and UpdateViewWindow (src/game/ui/ui_visitorStatus.cpp).
# The callees outside the unit are recording fakes in tests/unit_VisitorStatusPanel_UpdateStatusWindow_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_VisitorStatusPanel_UpdateStatusWindow_2 src/game/ui/ui_visitorStatus.cpp platform/arena.cpp)
set(TPW_UNIT_VisitorStatusPanel_UpdateStatusWindow_2_KEEP
  __ZN14visitor_callee13WindowSetTextEP15InterfaceWindowPKDs   # visitor_callee::WindowSetText(InterfaceWindow*, char16_t const*)
  __ZN14visitor_callee14WindowSetValueEP15InterfaceWindowi   # visitor_callee::WindowSetValue(InterfaceWindow*, int)
  __ZN14visitor_callee8RectCtorEPv   # visitor_callee::RectCtor(void*)
  __ZN14visitor_callee8RectDtorEPv   # visitor_callee::RectDtor(void*)
  __ZN18VisitorStatusPanel16UpdateViewWindowEv   # VisitorStatusPanel::UpdateViewWindow()
  __ZN18VisitorStatusPanel18UpdateStatusWindowEv   # VisitorStatusPanel::UpdateStatusWindow()
)
