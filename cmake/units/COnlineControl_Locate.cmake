# Unit v5023: COnlineControl chat commands (Locate, Page, Goto, BlackMark, BuddyOn; src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_COnlineControl_Locate.cpp.
set(TPW_UNIT_COnlineControl_Locate src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_Locate_KEEP
  __ZN14COnlineControl4GotoERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::Goto(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl4PageERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::Page(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl6LocateERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::Locate(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl7BuddyOnERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::BuddyOn(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl9BlackMarkERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::BlackMark(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
)
