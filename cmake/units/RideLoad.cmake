# Unit w167: RSSE_Load (the ride scripts' load) and RSSE_WALK::Byteswap (src/game/rides/rsse.cpp, with w166's RSSE_Save and the swap helpers). The arena is
# the allocator behind the Ptr32 links of the loaded records, tables and objects. The file read, the memory trace, the asserts, the script kill, the
# system's init, the word swap, the ride bound area and the sound call are fakes in tests/unit_RideLoad.cpp, which also defines the globals of
# other files and the ride sets.
set(TPW_UNIT_RideLoad src/game/rides/rsse.cpp platform/arena.cpp)
set(TPW_UNIT_RideLoad_KEEP
  __Z9RSSE_LoadPv   # RSSE_Load(void*)
  __Z9RSSE_SavePv   # RSSE_Save(void*)
  __ZN8RsseSwap14USwap32CharPtrEj   # RsseSwap::USwap32CharPtr(unsigned int)
  __ZN8RsseSwap14USwap32LongPtrEj   # RsseSwap::USwap32LongPtr(unsigned int)
  __ZN8RsseSwap14USwap32VoidPtrEj   # RsseSwap::USwap32VoidPtr(unsigned int)
  __ZN8RsseSwap14USwap32WalkPtrEj   # RsseSwap::USwap32WalkPtr(unsigned int)
  __ZN8RsseSwap16USwap32ObjectPtrEj   # RsseSwap::USwap32ObjectPtr(unsigned int)
  __ZN8RsseSwap17USwapInplace32IntERi   # RsseSwap::USwapInplace32Int(int&)
  __ZN8RsseSwap18USwapInplace32LongERi   # RsseSwap::USwapInplace32Long(int&)
  __ZN8RsseSwap18USwapInplace32UIntERj   # RsseSwap::USwapInplace32UInt(unsigned int&)
  __ZN8RsseSwap19USwapInplace16ShortERs   # RsseSwap::USwapInplace16Short(short&)
  __ZN8RsseSwap19USwapInplace32ULongERj   # RsseSwap::USwapInplace32ULong(unsigned int&)
  __ZN8RsseSwap20USwapInplace16UShortERt   # RsseSwap::USwapInplace16UShort(unsigned short&)
  __ZN9RSSE_WALK8ByteswapEv   # RSSE_WALK::Byteswap()
)
