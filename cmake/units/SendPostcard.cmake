# Unit v5884: the postcard panel's sending half (IllCallYouBack, AddressesValid, AddToAddressField, SendPostcard; src/game/ui/ui_Postcard.cpp).
# The callees outside the unit are recording fakes in tests/unit_SendPostcard.cpp, which also defines the globals of other files.
set(TPW_UNIT_SendPostcard src/game/ui/ui_Postcard.cpp platform/arena.cpp platform/msl.cpp)
set(TPW_UNIT_SendPostcard_KEEP
  __Z12SendPostcardv   # SendPostcard()
  __Z14AddressesValidv   # AddressesValid()
  __Z14IllCallYouBack19EmailCompletionCode14EmailErrorCode   # IllCallYouBack(EmailCompletionCode, EmailErrorCode)
  __Z17AddToAddressFieldPDs   # AddToAddressField(char16_t*)
  _gPostcardCounter
  _gPostcardJpgPath
  _kCSubscriptionHeaderCtorVector
  _kCSubscriptionHeaderDtorVector
  _kIllCallYouBackVector
)
