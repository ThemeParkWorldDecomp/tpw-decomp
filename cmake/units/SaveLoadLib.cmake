set(TPW_UNIT_SaveLoadLib src/bullfrog/LbZLib.cpp src/bullfrog/LbFile.cpp)
set(TPW_UNIT_SaveLoadLib_KEEP
  __Z24LbFile_CompareFileStampsPK11TbFileStampS1_   # LbFile_CompareFileStamps(TbFileStamp const*, TbFileStamp const*)
  __Z26LbZLib_MemDecompressedSizePv   # LbZLib_MemDecompressedSize(void*)
  __ZNK3bfl8compress5CZLib16DecompressedSizeEPh   # bfl::compress::CZLib::DecompressedSize(unsigned char*) const
)
