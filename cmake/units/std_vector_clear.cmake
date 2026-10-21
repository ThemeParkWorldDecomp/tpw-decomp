# Unit v5956: std::vector<Argument>::clear / tear_down, CToken constructors, CParser::GetChar, COnlinePerson::COnlinePerson and
# std::list<std::wstring>::insert (src/game/MslSupport.cpp, src/game/online/OnlineParser.cpp). The callees outside the
# unit are recording fakes in tests/unit_std_vector_clear.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_vector_clear src/game/MslSupport.cpp src/game/online/OnlineParser.cpp platform/arena.cpp)
set(TPW_UNIT_std_vector_clear_KEEP
  __ZN12token_callee10CTokenCtorEPvPKvjjj   # token_callee::CTokenCtor(void*, void const*, unsigned int, unsigned int, unsigned int)
  __ZN12wstring_list18WStringList_insertEPPNS_11WStringNodeEPNS_11WStringListES1_PKv   # wstring_list::WStringList_insert(wstring_list::WStringNode**, wstring_list::WStringList*, wstring_list::WStringNode*, void const*)
  __ZN13online_callee17COnlinePersonCtorEPv12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtEjS0_b   # online_callee::COnlinePersonCtor(void*, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>, unsigned int, void*, bool)
  __ZN13online_callee22ArgumentVectorTearDownEPv   # online_callee::ArgumentVectorTearDown(void*)
  __ZN19online_parse_callee13ParserGetCharEPvPDs   # online_parse_callee::ParserGetChar(void*, char16_t*)
  __ZN20online_parser_callee19ArgumentVectorClearEPv   # online_parser_callee::ArgumentVectorClear(void*)
  __ZN6CTokenC1ERKS_   # CToken::CToken(CToken const&)
)
