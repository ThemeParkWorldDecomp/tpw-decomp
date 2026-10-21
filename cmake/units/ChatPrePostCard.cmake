# Unit v5579: the chat postcard hand-over, the outbox static initialiser and the JPEG error exit (src/game/ui/ui_onlineOutbox.cpp, src/game/ui/ui_onlinePanel.cpp).
# The callees outside the unit are recording fakes in tests/unit_ChatPrePostCard.cpp, which also defines the globals of other files.
set(TPW_UNIT_ChatPrePostCard src/game/ui/ui_onlineOutbox.cpp src/game/ui/ui_onlinePanel.cpp platform/arena.cpp)
set(TPW_UNIT_ChatPrePostCard_KEEP
  __Z13my_error_exitP18jpeg_common_struct   # my_error_exit(jpeg_common_struct*)
  __Z15ChatPrePostCardv   # ChatPrePostCard()
  __Z26ChatReinstateAfterPostcardv   # ChatReinstateAfterPostcard()
  __Z27__sinit_ui_onlineOutbox_cppv   # __sinit_ui_onlineOutbox_cpp()
  __ZN11init_callee16LockChatPanelOffEv   # init_callee::LockChatPanelOff()
  _gOnlineSavedWindow84
  _gOnlineWord80
  _gOutboxStringNode
  _gOutboxStringStorage
)
