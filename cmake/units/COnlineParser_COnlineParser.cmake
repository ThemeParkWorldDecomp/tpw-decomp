# Unit v5131: COnlineParser constructor, destructor, ParseEarmuffs, ParseAfk, ParseVote, ParseFilter
# (src/game/online/OnlineParser.cpp). The callees are recording fakes in tests/unit_COnlineParser_COnlineParser.cpp.
set(TPW_UNIT_COnlineParser_COnlineParser src/game/online/OnlineParser.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineParser_COnlineParser_KEEP
  __ZN13COnlineParser11ParseFilterEv   # COnlineParser::ParseFilter()
  __ZN13COnlineParser13ParseEarmuffsEv   # COnlineParser::ParseEarmuffs()
  __ZN13COnlineParser8DestructEPS_s   # COnlineParser::Destruct(COnlineParser*, short)
  __ZN13COnlineParser8ParseAfkEv   # COnlineParser::ParseAfk()
  __ZN13COnlineParser9ConstructEPS_PvS1_   # COnlineParser::Construct(COnlineParser*, void*, void*)
  __ZN13COnlineParser9ParseVoteEv   # COnlineParser::ParseVote()
  _gOnlineParserInitialText
)
