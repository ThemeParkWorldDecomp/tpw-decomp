# Unit v5132: std::list<CToken>::clear (src/game/online/OnlineParser.cpp). operator delete is a recording fake in
# tests/unit_std_list_clear.cpp.
set(TPW_UNIT_std_list_clear src/game/online/OnlineParser.cpp platform/arena.cpp)
set(TPW_UNIT_std_list_clear_KEEP
  __Z15CTokenListClearP10CTokenList   # CTokenListClear(CTokenList*)
)
