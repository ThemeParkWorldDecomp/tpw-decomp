# Unit v5051: COnlineParser::ParseBlackMark, ParseBuddy, ParsePage, ParseGoto, ParseLocate (src/game/online/OnlineParser.cpp). The callees are recording fakes in tests/unit_COnlineParser_ParseGoto.cpp.
set(TPW_UNIT_COnlineParser_ParseGoto src/game/online/OnlineParser.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineParser_ParseGoto_KEEP
  __ZN13COnlineParser10ParseBuddyEv   # COnlineParser::ParseBuddy()
  __ZN13COnlineParser11ParseLocateEv   # COnlineParser::ParseLocate()
  __ZN13COnlineParser14ParseBlackMarkEv   # COnlineParser::ParseBlackMark()
  __ZN13COnlineParser9ParseGotoEv   # COnlineParser::ParseGoto()
  __ZN13COnlineParser9ParsePageEv   # COnlineParser::ParsePage()
)
