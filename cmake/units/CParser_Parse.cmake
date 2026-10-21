# Unit v5911: CParser's nine functions (src/game/online/Parser.cpp).
# The callees outside the unit are recording fakes in tests/unit_CParser_Parse.cpp, which also defines the globals of other files.
set(TPW_UNIT_CParser_Parse src/game/online/Parser.cpp platform/arena.cpp tests/standins/CParser_Parse.cpp)
set(TPW_UNIT_CParser_Parse_KEEP
  __ZN14cparser_callee10StringDtorEPvs   # cparser_callee::StringDtor(void*, short)
  __ZN14cparser_callee11CallHandlerEP7CParserPKj   # cparser_callee::CallHandler(CParser*, unsigned int const*)
  __ZN14cparser_callee14OperatorDeleteEPv   # cparser_callee::OperatorDelete(void*)
  __ZN7CParser14SkipWhitespaceEv   # CParser::SkipWhitespace()
  __ZN7CParser15SetupForParsingEPKvPvPKh   # CParser::SetupForParsing(void const*, void*, unsigned char const*)
  __ZN7CParser18CountNonWhitespaceEv   # CParser::CountNonWhitespace()
  __ZN7CParser18GetRemainingStringEPvPi   # CParser::GetRemainingString(void*, int*)
  __ZN7CParser26SkipWhitespaceAndSeparatorEv   # CParser::SkipWhitespaceAndSeparator()
  __ZN7CParser5MatchEPKv   # CParser::Match(void const*)
  __ZN7CParser5ParseEv   # CParser::Parse()
  __ZN7CParser8DestructEPS_s   # CParser::Destruct(CParser*, short)
  __ZN7CParser9GetStringEPv   # CParser::GetString(void*)
)
