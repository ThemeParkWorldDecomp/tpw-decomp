# Unit v5555: COnlineParser::ParseIgnore and DoParse (src/game/online/OnlineParser.cpp). The callees are recording fakes in
# tests/unit_COnlineParser_ParseIgnore.cpp.
set(TPW_UNIT_COnlineParser_ParseIgnore src/game/online/OnlineParser.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineParser_ParseIgnore_KEEP
  __ZN13COnlineParser11ParseIgnoreEv   # COnlineParser::ParseIgnore()
  __ZN13COnlineParser7DoParseEPKv   # COnlineParser::DoParse(void const*)
)
