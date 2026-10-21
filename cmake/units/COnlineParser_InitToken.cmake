# Unit v5187: COnlineParser::InitToken, InitTokenList, ParseSay, ParseTell (src/game/online/OnlineParser.cpp). The callees
# outside the unit are recording fakes in tests/unit_COnlineParser_InitToken.cpp, which also defines gText and the handler table.
set(TPW_UNIT_COnlineParser_InitToken src/game/online/OnlineParser.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineParser_InitToken_KEEP
  __ZN13COnlineParser13InitTokenListEv   # COnlineParser::InitTokenList()
  __ZN13COnlineParser8ParseSayEv   # COnlineParser::ParseSay()
  __ZN13COnlineParser9InitTokenEj13ParserHandler   # COnlineParser::InitToken(unsigned int, ParserHandler)
  __ZN13COnlineParser9ParseTellEv   # COnlineParser::ParseTell()
)
