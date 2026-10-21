# Unit v5145: the lenders list of the bank panel (FillLenderList, LCFormat, LCPostFormat; src/game/ui/ui_loansPanel.cpp). The callees outside the
# unit are recording fakes in tests/unit_FillLenderList.cpp, which also defines the globals of other files.
set(TPW_UNIT_FillLenderList src/game/ui/ui_loansPanel.cpp platform/arena.cpp)
set(TPW_UNIT_FillLenderList_KEEP
  __Z14FillLenderListP21InterfaceListControl2   # FillLenderList(InterfaceListControl2*)
  __Z14LenderLCFormatv   # LenderLCFormat()
  __Z18LenderLCPostFormatP15InterfaceWindow   # LenderLCPostFormat(InterfaceWindow*)
  __ZN13lender_callee10NumberCtorEPv   # lender_callee::NumberCtor(void*)
  __ZN13lender_callee10SpriteCtorEPv   # lender_callee::SpriteCtor(void*)
  __ZN13lender_callee10WindowCtorEPvP15InterfaceWindowiissss   # lender_callee::WindowCtor(void*, InterfaceWindow*, int, int, short, short, short, short)
  __ZN13lender_callee11OperatorNewEj   # lender_callee::OperatorNew(unsigned int)
  __ZN13lender_callee11PercentCtorEPv   # lender_callee::PercentCtor(void*)
  __ZN13lender_callee14WindowSetValueEP15InterfaceWindowi   # lender_callee::WindowSetValue(InterfaceWindow*, int)
  __ZN13lender_callee8CashCtorEPv   # lender_callee::CashCtor(void*)
  _gLenderFontIndex
  _gLenderListMode
  _gLenderRowHeight
)
