# Unit v5897: COnlineControl IgnoreOff, IgnoreOn, BuddyOff, SendText, RemoveContact, RemoveBuddy, UpdateUserList
# (src/game/online/online.cpp). The callees outside the
# unit are recording fakes in tests/unit_COnlineControl_IgnoreOff.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_IgnoreOff src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_IgnoreOff_KEEP
  __ZN14COnlineControl11RemoveBuddyERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::RemoveBuddy(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl13RemoveContactERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::RemoveContact(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl8BuddyOffERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::BuddyOff(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl8IgnoreOnERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::IgnoreOn(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl8SendTextENSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::SendText(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>)
  __ZN14COnlineControl9IgnoreOffERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::IgnoreOff(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZNK14COnlineControl14UpdateUserListEv   # COnlineControl::UpdateUserList() const
)
