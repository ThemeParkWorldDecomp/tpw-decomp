# Unit v5285: COnlineControl's avatar moves, chat text and contact lookups (CreateVisitor, SetAvatarDestination, TeleportAvatar, IsThisMe,
# ReceiveText, Feedback, GetDataForUser, AddContact; src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_COnlineControl_CreateVisitor.cpp, which also defines the globals of other files.
set(TPW_UNIT_COnlineControl_CreateVisitor src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_COnlineControl_CreateVisitor_KEEP
  __ZN14COnlineControl10AddContactERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEEi   # COnlineControl::AddContact(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&, int)
  __ZN14COnlineControl11ReceiveTextERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEEi   # COnlineControl::ReceiveText(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&, int)
  __ZN14COnlineControl13CreateVisitorERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEEiiii   # COnlineControl::CreateVisitor(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&, int, int, int, int)
  __ZN14COnlineControl14TeleportAvatarE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # COnlineControl::TeleportAvatar(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN14COnlineControl20SetAvatarDestinationE12tag_IDX_TYPEI22IDXTYPEUniqueNameMapIdtE   # COnlineControl::SetAvatarDestination(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, unsigned short>)
  __ZN14COnlineControl8FeedbackERNSt3__112basic_stringIDsNS0_11char_traitsIDsEENS0_9allocatorIDsEEEE   # COnlineControl::Feedback(std::__1::basic_string<char16_t, std::__1::char_traits<char16_t>, std::__1::allocator<char16_t>>&)
  __ZN14COnlineControl8IsThisMeEPKDs   # COnlineControl::IsThisMe(char16_t const*)
  __ZNK14COnlineControl14GetDataForUserERK13nocase_string   # COnlineControl::GetDataForUser(nocase_string const&) const
)
