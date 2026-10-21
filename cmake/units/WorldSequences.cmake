# The world's sequences and small helpers (unit w194): CWorld::PopulatePark, StartingPopulationForOnline, StartBankruptcySequence,
# StartWinningSequence, IsPlayingWinningAnim, EndWinningSequence, GetPrevUsedThing, SeedRand, ReconstructFreeList, GetNewPersonIndex,
# FlagItemAsSelected, ConvertDLRidesUsedToIndices, RestoreEngineObjects, DeleteEngineObjects and the pending list's RemoveHead and AddHead.
# Every callee outside them (the things' constructors and virtuals, the map, the analyser, the ride database, the gates, the windows) is a
# fake in the unit file.
set(TPW_UNIT_WorldSequences src/game/World.cpp)
set(TPW_UNIT_WorldSequences_KEEP
  __ZN6CWorld12PopulateParkEv   # CWorld::PopulatePark()
  __ZN6CWorld17GetNewPersonIndexEv   # CWorld::GetNewPersonIndex()
  __ZN6CWorld18EndWinningSequenceEv   # CWorld::EndWinningSequence()
  __ZN6CWorld18FlagItemAsSelectedE12tag_IDX_TYPEI25IDXTYPEUniqueNameObjectIdtE   # CWorld::FlagItemAsSelected(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>)
  __ZN6CWorld19DeleteEngineObjectsEv   # CWorld::DeleteEngineObjects()
  __ZN6CWorld19ReconstructFreeListEv   # CWorld::ReconstructFreeList()
  __ZN6CWorld20IsPlayingWinningAnimEv   # CWorld::IsPlayingWinningAnim()
  __ZN6CWorld20RestoreEngineObjectsEv   # CWorld::RestoreEngineObjects()
  __ZN6CWorld20StartWinningSequenceEv   # CWorld::StartWinningSequence()
  __ZN6CWorld23StartBankruptcySequenceEv   # CWorld::StartBankruptcySequence()
  __ZN6CWorld27ConvertDLRidesUsedToIndicesEPt   # CWorld::ConvertDLRidesUsedToIndices(unsigned short*)
  __ZN6CWorld27StartingPopulationForOnlineEv   # CWorld::StartingPopulationForOnline()
  __ZN6CWorld8SeedRandEj   # CWorld::SeedRand(unsigned int)
  __ZN7TbTListI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES2_E10RemoveHeadEv   # TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>::RemoveHead()
  __ZN7TbTListI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES2_E7AddHeadES2_   # TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>::AddHead(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZNK6CWorld16GetPrevUsedThingEPK6CThing   # CWorld::GetPrevUsedThing(CThing const*) const
  _gWinningAnimThing
  _gWorldEndName
)
# The unit file includes calllog.cpp itself (it keeps the blocks the code frees until the next case).
set(TPW_UNIT_WorldSequences_OWN_CALLLOG ON)
