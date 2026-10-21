# Unit v5069: std::list<CToken>::insert (src/game/online/OnlineParser.cpp). The callees are recording fakes in
# tests/unit_std_list_insert.cpp, which also defines the library globals.
set(TPW_UNIT_std_list_insert src/game/online/OnlineParser.cpp platform/arena.cpp)
set(TPW_UNIT_std_list_insert_KEEP
  __ZN10token_list16TokenList_insertEPPNS_13TokenListNodeEPNS_9TokenListES1_PK6CToken   # token_list::TokenList_insert(token_list::TokenListNode**, token_list::TokenList*, token_list::TokenListNode*, CToken const*)
)
