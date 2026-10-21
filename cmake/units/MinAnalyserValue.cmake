# Unit v5135: analyser min/max helpers and the errorthingy cursor += (src/game/ui/ui_graph.cpp). Callees are fakes in tests/unit_MinAnalyserValue.cpp.
set(TPW_UNIT_MinAnalyserValue src/game/ui/ui_graph.cpp platform/arena.cpp)
set(TPW_UNIT_MinAnalyserValue_KEEP
  __Z16MaxAnalyserValueP8CHistoryIfLi144EERii   # MaxAnalyserValue(CHistory<float, 144>*, int&, int)
  __Z16MaxAnalyserValueP8CHistoryIiLi144EERii   # MaxAnalyserValue(CHistory<int, 144>*, int&, int)
  __Z16MinAnalyserValueP8CHistoryIiLi144EERii   # MinAnalyserValue(CHistory<int, 144>*, int&, int)
  __Z33MslErrorDequeCursorAddErrorthingyP19MslErrorDequeCursori   # MslErrorDequeCursorAddErrorthingy(MslErrorDequeCursor*, int)
  _gMslMacAddressHigh
)
