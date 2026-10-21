# Unit v51059: CParser's set helpers, the std::set<wchar_t> tree functions, the online set/map destructors and small functions
# (src/game/online/OnlinePerson.cpp, src/game/online/Parser.cpp, src/game/online/online.cpp). The callees outside the unit are
# recording fakes in tests/unit_CParser_IgnoreRemainingString.cpp.
set(TPW_UNIT_CParser_IgnoreRemainingString src/game/online/OnlinePerson.cpp src/game/online/Parser.cpp
    src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_CParser_IgnoreRemainingString_KEEP
  __ZN11std_destroy17DestroyRangeClearEP8ArgumentS1_   # std_destroy::DestroyRangeClear(Argument*, Argument*)
  __ZN13COnlinePerson11ProcessIdleEv   # COnlinePerson::ProcessIdle()
  __ZN13COnlinePerson12FinalDestroyEv   # COnlinePerson::FinalDestroy()
  __ZN13COnlinePerson14ReceiveMessageERK4CMsg   # COnlinePerson::ReceiveMessage(CMsg const&)
  __ZN13online_callee12BuddyMapDtorEPvs   # online_callee::BuddyMapDtor(void*, short)
  __ZN13online_callee13NocaseSetDtorEPvs   # online_callee::NocaseSetDtor(void*, short)
  __ZN13online_callee14WStringSetDtorEPvs   # online_callee::WStringSetDtor(void*, short)
  __ZN13parser_callee8TreeCtorEPvPKvS2_   # parser_callee::TreeCtor(void*, void const*, void const*)
  __ZN14cparser_callee11TreeDestroyEPvS0_   # cparser_callee::TreeDestroy(void*, void*)
  __ZN14cparser_callee8TreeFindEPvPKDs   # cparser_callee::TreeFind(void*, char16_t const*)
  __ZN20online_parser_callee8ListCtorEPv   # online_parser_callee::ListCtor(void*)
  __ZN6v5105912IPSystemDtorEPvs   # v51059::IPSystemDtor(void*, short)
  __ZN6v5105912WcharSetDtorEP12WcharSetTrees   # v51059::WcharSetDtor(WcharSetTree*, short)
  __ZN6v5105913TreeInsertOneEPNS_17WcharInsertResultEP12WcharSetTreePKDs   # v51059::TreeInsertOne(v51059::WcharInsertResult*, WcharSetTree*, char16_t const*)
  __ZN6v5105914NodeDeallocateEPvPN12WcharSetTree4nodeEj   # v51059::NodeDeallocate(void*, WcharSetTree::node*, unsigned int)
  __ZN6v5105921WcharAllocatorDestroyEPvPDs   # v51059::WcharAllocatorDestroy(void*, char16_t*)
  __ZN7CParser12AddSeparatorEDs   # CParser::AddSeparator(char16_t)
  __ZN7CParser13AddWhitespaceEDs   # CParser::AddWhitespace(char16_t)
  __ZN7CParser17SetupSeparatorSetEv   # CParser::SetupSeparatorSet()
  __ZN7CParser21IgnoreRemainingStringEv   # CParser::IgnoreRemainingString()
  _gIPBaseVtableBlock
  _gIPSystemVtableBlock
  _gTokenListInit
  _gWcharSetCountInit
  _gWcharSetInserted
  _gWcharSetNotInserted
)
