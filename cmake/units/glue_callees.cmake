# Unit v5L49: the weak library-glue callees (src/game/online/online.cpp), run against the Mac's MSL and NS_MacDoze by tests/test_glue_callees.py.
set(TPW_UNIT_glue_callees src/game/online/online.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_glue_callees_KEEP
  __ZN13ignore_callee11SprintfWideEPcPKcPKDs   # ignore_callee::SprintfWide(char*, char const*, char16_t const*)
  __ZN14textobj_callee6WcsSetEPDsDs   # textobj_callee::WcsSet(char16_t*, char16_t)
  __ZN15wostream_callee6WcslenEPKDs   # wostream_callee::Wcslen(char16_t const*)
  __ZN20online_parser_callee6WcsLenEPKDs   # online_parser_callee::WcsLen(char16_t const*)
)
