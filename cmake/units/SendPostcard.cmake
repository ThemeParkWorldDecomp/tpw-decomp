# Unit v5884: the postcard panel's sending half (IllCallYouBack, AddressesValid, AddToAddressField, SendPostcard; src/ui_Postcard_panel_SendPostcard.cpp).
# The callees outside the unit are recording fakes in tests/unit_SendPostcard.cpp, which also defines the globals of other files.
set(TPW_UNIT_SendPostcard src/ui_Postcard_panel_SendPostcard.cpp platform/arena.cpp platform/msl.cpp)
