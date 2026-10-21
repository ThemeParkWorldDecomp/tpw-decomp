# Unit v5891: COnlineParser ParseReply, ParseLast, ParseShout, ParseEmote, ParseEmoteTell (src/game/online/OnlineParser.cpp).
# The callees are recording fakes in tests/unit_COnlineParser_ParseReply_2.cpp.
set(TPW_UNIT_COnlineParser_ParseReply_2 src/game/online/OnlineParser.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineParser_ParseReply_2_KEEP
  __ZN13COnlineParser10ParseEmoteEv   # COnlineParser::ParseEmote()
  __ZN13COnlineParser10ParseReplyEv   # COnlineParser::ParseReply()
  __ZN13COnlineParser10ParseShoutEv   # COnlineParser::ParseShout()
  __ZN13COnlineParser14ParseEmoteTellEv   # COnlineParser::ParseEmoteTell()
  __ZN13COnlineParser9ParseLastEv   # COnlineParser::ParseLast()
)
