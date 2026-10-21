# Unit v51039: std::list<std::wstring>::clear, SwearFilter and theme map destructors, InterfaceFontWrapper::GetCharWidth,
# CFSOnlineSystemParkList::StoreReceivedList and four UI helpers (src/game/online/FS_OnlineList.cpp, src/game/online/OnlineParser.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/ui_main.cpp).
# The callees outside the unit are recording fakes in tests/unit_std_list_std_basic_string_std_allocator_std_basi_4.cpp, which also
# defines the globals of other files.
set(TPW_UNIT_std_list_std_basic_string_std_allocator_std_basi_4 src/game/online/FS_OnlineList.cpp
    src/game/online/OnlineParser.cpp src/game/ui/InterfaceFont.cpp src/game/ui/ui_main.cpp platform/arena.cpp)
set(TPW_UNIT_std_list_std_basic_string_std_allocator_std_basi_4_KEEP
  __Z16WStringListClearP14MacWStringList   # WStringListClear(MacWStringList*)
  __Z19UIMain_IndentClientP15InterfaceWindowss   # UIMain_IndentClient(InterfaceWindow*, short, short)
  __Z24UIMain_SetStaffStateTextP15InterfaceWindowi   # UIMain_SetStaffStateText(InterfaceWindow*, int)
  __Z25UIMain_GetStaffStateFramei   # UIMain_GetStaffStateFrame(int)
  __ZN11SwearFilter8DestructEPS_s   # SwearFilter::Destruct(SwearFilter*, short)
  __ZN13ParkThemeTree8DestructEPS_s   # ParkThemeTree::Destruct(ParkThemeTree*, short)
  __ZN13indent_callee6AdjustEPvssss   # indent_callee::Adjust(void*, short, short, short, short)
  __ZN13indent_callee8RectDtorEPvs   # indent_callee::RectDtor(void*, short)
  __ZN15parklist_callee7NewParkEPK10ccParkInfoP23CFSOnlineSystemParkList   # parklist_callee::NewPark(ccParkInfo const*, CFSOnlineSystemParkList*)
  __ZN17masterlist_callee11WStringDtorEPvs   # masterlist_callee::WStringDtor(void*, short)
  __ZN17masterlist_callee14OperatorDeleteEPv   # masterlist_callee::OperatorDelete(void*)
  __ZN18fontwrapper_callee12GetCharIndexEP7TbIFontRPKh   # fontwrapper_callee::GetCharIndex(TbIFont*, unsigned char const*&)
  __ZN18fontwrapper_callee14GetCharWidthOfEP7TbIFontt   # fontwrapper_callee::GetCharWidthOf(TbIFont*, unsigned short)
  __ZN20InterfaceFontWrapper12GetCharWidthEDs   # InterfaceFontWrapper::GetCharWidth(char16_t)
  __ZN23CFSOnlineSystemParkList17StoreReceivedListEPK14ccReceivedListI10ccParkInfoE   # CFSOnlineSystemParkList::StoreReceivedList(ccReceivedList<ccParkInfo> const*)
  TPW_V51039_STRONG@game/online/FS_OnlineList.cpp   # the #define TPW_V51039_STRONG of src/game/online/FS_OnlineList.cpp (a guard macro; src/game/online/FS_OnlineList.cpp, src/game/online/OnlineParser.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/ui_main.cpp had it)
  TPW_V51039_STRONG@game/online/OnlineParser.cpp   # the #define TPW_V51039_STRONG of src/game/online/OnlineParser.cpp (a guard macro; src/game/online/FS_OnlineList.cpp, src/game/online/OnlineParser.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/ui_main.cpp had it)
  TPW_V51039_STRONG@game/ui/InterfaceFont.cpp   # the #define TPW_V51039_STRONG of src/game/ui/InterfaceFont.cpp (a guard macro; src/game/online/FS_OnlineList.cpp, src/game/online/OnlineParser.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/ui_main.cpp had it)
  TPW_V51039_STRONG@game/ui/ui_main.cpp   # the #define TPW_V51039_STRONG of src/game/ui/ui_main.cpp (a guard macro; src/game/online/FS_OnlineList.cpp, src/game/online/OnlineParser.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/ui_main.cpp had it)
)
