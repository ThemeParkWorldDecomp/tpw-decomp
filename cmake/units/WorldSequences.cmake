# The world's sequences and small helpers (unit w194): CWorld::PopulatePark, StartingPopulationForOnline, StartBankruptcySequence,
# StartWinningSequence, IsPlayingWinningAnim, EndWinningSequence, GetPrevUsedThing, SeedRand, ReconstructFreeList, GetNewPersonIndex,
# FlagItemAsSelected, ConvertDLRidesUsedToIndices, RestoreEngineObjects, DeleteEngineObjects and the pending list's RemoveHead and AddHead.
# Every callee outside them (the things' constructors and virtuals, the map, the analyser, the ride database, the gates, the windows) is a
# fake in the unit file.
set(TPW_UNIT_WorldSequences src/World_sequences.cpp)
# The unit file includes calllog.cpp itself (it keeps the blocks the code frees until the next case).
set(TPW_UNIT_WorldSequences_OWN_CALLLOG ON)
