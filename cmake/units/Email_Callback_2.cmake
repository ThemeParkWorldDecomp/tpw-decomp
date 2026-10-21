# Unit v5912: Email::Email and Email::Callback (src/game/online/EmailMessage.cpp). The callees outside the unit are recording fakes in
# tests/unit_Email_Callback_2.cpp.
set(TPW_UNIT_Email_Callback_2 src/game/online/EmailMessage.cpp platform/arena.cpp)
set(TPW_UNIT_Email_Callback_2_KEEP
  __ZN5Email8CallbackEjiPh   # Email::Callback(unsigned int, int, unsigned char*)
  __ZN5Email9ConstructEPS_   # Email::Construct(Email*)
)
