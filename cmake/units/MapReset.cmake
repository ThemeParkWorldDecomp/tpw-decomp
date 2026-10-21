# The map reset and the attribute map (unit w112c): LLCC_ResetMap, the map and track cell resets, HFieldCellStatus, RemoveNeighbour
# and ModifyDirection; the chunk file reader with its handlers, the list members it uses and CMap::LoadAttribMap; the script
# system's check table. The unit file includes calllog.cpp itself (it records the heap traffic of the list nodes and the table).
# platform/arena.cpp gives the height field's file image structures their Ptr32 targets.
set(TPW_UNIT_MapReset src/game/park/CellBase.cpp src/game/park/LLCellSystem.cpp src/game/park/MapCell.cpp
    src/game/park/TPHField.cpp src/game/park/TrackCell.cpp src/game/World.cpp src/game/park/Map.cpp
    src/game/save/ChunkFileReader.cpp src/game/Script.cpp platform/arena.cpp)
set(TPW_UNIT_MapReset_KEEP
  __Z13LLCC_ResetMapii   # LLCC_ResetMap(int, int)
  __Z16HFieldCellStatusii   # HFieldCellStatus(int, int)
  __Z18SCR_InitCheckTablev   # SCR_InitCheckTable()
  __Z19SCR_CloseCheckTablev   # SCR_CloseCheckTable()
  __ZN10CTrackCell5ResetEv   # CTrackCell::Reset()
  __ZN16CChunkFileReader15AddChunkHandlerEP23CIndividualChunkHandler   # CChunkFileReader::AddChunkHandler(CIndividualChunkHandler*)
  __ZN16CChunkFileReader8ReadFileEPKc   # CChunkFileReader::ReadFile(char const*)
  __ZN16CChunkFileReader8ReadFileERN3bfl10FileSystem5CFileEPc   # CChunkFileReader::ReadFile(bfl::FileSystem::CFile&, char*)
  __ZN16CChunkFileReaderD1Ev   # CChunkFileReader::~CChunkFileReader()
  __ZN16CMapChunkHandler10ParseChunkEPvj   # CMapChunkHandler::ParseChunk(void*, unsigned int)
  __ZN20CMapAttribFileReaderD1Ev   # CMapAttribFileReader::~CMapAttribFileReader()
  __ZN23CIndividualChunkHandler13IsThisMyChunkEj   # CIndividualChunkHandler::IsThisMyChunk(unsigned int)
  __ZN4CMap13LoadAttribMapEPKc   # CMap::LoadAttribMap(char const*)
  __ZN8CMapCell5ResetEb   # CMapCell::Reset(bool)
  __ZN9CCellBase15ModifyDirectionEh   # CCellBase::ModifyDirection(unsigned char)
  __ZN9CCellBase15RemoveNeighbourEh   # CCellBase::RemoveNeighbour(unsigned char)
  _gScriptCheckTable
  _gScriptCodeWords
  _gScriptCodeWordsEnd
  _gScriptCommandFirst
  _gScriptCommandList
  __ZTS7TbTListIP23CIndividualChunkHandlerS1_E   # template class TbTList<CIndividualChunkHandler*, CIndividualChunkHandler*> (explicit instantiation)
  __ZTS9TbPtrListI23CIndividualChunkHandlerE   # template class TbPtrList<CIndividualChunkHandler> (explicit instantiation)
  TPW_LLCC_DRAWTRACKLINE_R3@game/park/TrackCell.cpp   # the #define TPW_LLCC_DRAWTRACKLINE_R3 of src/game/park/TrackCell.cpp (a guard macro the regroup brought into it)
)
set(TPW_UNIT_MapReset_OWN_CALLLOG ON)
