# Unit v5903: buddy / ignore list helpers, GotoFailed, GotoStoredPerson, pair::~pair, basic_ofstream::~basic_ofstream
# (src/game/online/online.cpp).
# The callees outside the unit are recording fakes in tests/unit_std_basic_ofstream_Dtor_basic_ofstream_3.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_ofstream_Dtor_basic_ofstream_3 src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_ofstream_Dtor_basic_ofstream_3_KEEP
  __Z18BuddyEntryDestructP10BuddyEntrys   # BuddyEntryDestruct(BuddyEntry*, short)
  __ZN14COnlineControl10GotoFailedEv   # COnlineControl::GotoFailed()
  __ZN14COnlineControl16GotoStoredPersonEv   # COnlineControl::GotoStoredPerson()
  __ZN14COnlineControl23UpdateBuddyListToServerEv   # COnlineControl::UpdateBuddyListToServer()
  __ZN14COnlineControl24UpdateIgnoreListToServerEv   # COnlineControl::UpdateIgnoreListToServer()
  __ZN18BasicOfstreamWchar7destroyEs   # BasicOfstreamWchar::destroy(short)
  __ZNK14COnlineControl15UpdateBuddyListEv   # COnlineControl::UpdateBuddyList() const
  _gOfsWVtblB
)
