# Unit v5996: std::__putnumber<wchar_t>, basic_ios<wchar_t>::widen, deque<vector<Argument>>::const_iterator::operator++
# (src/game/MslSupport.cpp). The callees are recording fakes in the unit's test library.
set(TPW_UNIT_std___putnumber_wchar_t_std_ostreambuf_iterator src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___putnumber_wchar_t_std_ostreambuf_iterator_KEEP
  __ZN10wostream_u5WidenEPvc   # wostream_u::Widen(void*, char)
  __ZN15argdeque_callee7IterIncEP20MslArgumentDequeIter   # argdeque_callee::IterInc(MslArgumentDequeIter*)
  __ZN15wistream_callee5WidenEPvc   # wistream_callee::Widen(void*, char)
  __ZN15wostream_callee9PutNumberEPvS0_DsPKDsiS2_i   # wostream_callee::PutNumber(void*, void*, char16_t, char16_t const*, int, char16_t const*, int)
)
