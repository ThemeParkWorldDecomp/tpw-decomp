# The online panels' font and window virtuals as the program links them: contacts_callee's strong definitions in
# src/game/GameShimGlue.cpp and the real bodies they reach, on objects with their real vtables. The classes' other members are fakes in the unit file.
set(TPW_UNIT_ContactsSlots src/game/GameShimGlue.cpp src/game/ui/InterfaceFont.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/InterfaceButton.cpp)
set(TPW_UNIT_ContactsSlots_KEEP
  __ZN15contacts_callee13FontCharWidthEP13InterfaceFontDs   # contacts_callee::FontCharWidth(InterfaceFont*, char16_t)
  __ZN13InterfaceFont12GetCharWidthEDs   # InterfaceFont::GetCharWidth(char16_t)
  __ZN20InterfaceFontWrapper12GetCharWidthEDs   # InterfaceFontWrapper::GetCharWidth(char16_t)
  __ZN15contacts_callee12WindowSlot1cEP15InterfaceWindowi   # contacts_callee::WindowSlot1c(InterfaceWindow*, int)
  __ZN15InterfaceWindow6EnableEs   # InterfaceWindow::Enable(short)
  __ZN15InterfaceButton6EnableEs   # InterfaceButton::Enable(short)
  __ZN15InterfaceButton9CalcFrameEv   # InterfaceButton::CalcFrame()
  TPW_V51039_STRONG@game/ui/InterfaceFont.cpp   # the #define TPW_V51039_STRONG of src/game/ui/InterfaceFont.cpp
)
