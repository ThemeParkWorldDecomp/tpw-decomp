# Unit w216: stamps, packing and buffered unpacking with the real host zlib.
set(TPW_UNIT_StampZLib src/bullfrog/TbTimeStamp.cpp src/bullfrog/LbZLib.cpp)
set(TPW_UNIT_StampZLib_KEEP
  __Z10SwapHeaderP14ZLibCompHeader   # SwapHeader(ZLibCompHeader*)
  __Z18LbZLib_MemCompressPvS_jS_   # LbZLib_MemCompress(void*, void*, unsigned int, void*)
  __Z20LbZLib_MemDecompressPvS_S_   # LbZLib_MemDecompress(void*, void*, void*)
  __Z23LbZLib_BufferedCompressPvS_jjPFiS_jS_ES_S_   # LbZLib_BufferedCompress(void*, void*, unsigned int, unsigned int, int (*)(void*, unsigned int, void*), void*, void*)
  __Z25LbZLib_BufferedDecompressPvS_jPFiS_jS_ES_S_   # LbZLib_BufferedDecompress(void*, void*, unsigned int, int (*)(void*, unsigned int, void*), void*, void*)
  __Z28LbZLib_CompressWorkspaceSizev   # LbZLib_CompressWorkspaceSize()
  __Z30LbZLib_DeCompressWorkspaceSizev   # LbZLib_DeCompressWorkspaceSize()
  __ZN11TbTimeStamp7SetTimeEiiiiiii   # TbTimeStamp::SetTime(int, int, int, int, int, int, int)
  __ZN11TbTimeStamp8AddYearsEi   # TbTimeStamp::AddYears(int)
  __ZN11TbTimeStamp9AddMonthsEi   # TbTimeStamp::AddMonths(int)
  __ZN11TbTimeStampaSERK11TbFileStamp   # TbTimeStamp::operator=(TbFileStamp const&)
  __ZN11TbTimeStampaSERN10NS_MacDoze11tagFILETIMEE   # TbTimeStamp::operator=(NS_MacDoze::tagFILETIME&)
  __ZN11TbTimeStampmIERK10TbTimeDiff   # TbTimeStamp::operator-=(TbTimeDiff const&)
  __ZN11TbTimeStamppLERK10TbTimeDiff   # TbTimeStamp::operator+=(TbTimeDiff const&)
  __ZN3bfl8compress15CBufferCallbackD1Ev   # bfl::compress::CBufferCallback::~CBufferCallback()
  __ZN3bfl8compress24CBufferedCompressionBaseD1Ev   # bfl::compress::CBufferedCompressionBase::~CBufferedCompressionBase()
  __ZN3bfl8compress9Callbacks10BufferFullEPhj   # bfl::compress::Callbacks::BufferFull(unsigned char*, unsigned int)
  __ZN3bfl8compress9Callbacks11BufferEmptyEPhj   # bfl::compress::Callbacks::BufferEmpty(unsigned char*, unsigned int)
  __ZN3bfl8compress9CallbacksD1Ev   # bfl::compress::Callbacks::~Callbacks()
  __ZNK11TbTimeStamp7GetTimeEPiS0_S0_S0_S0_S0_S0_S0_   # TbTimeStamp::GetTime(int*, int*, int*, int*, int*, int*, int*, int*) const
  __ZNK11TbTimeStampeqERKS_   # TbTimeStamp::operator==(TbTimeStamp const&) const
  __ZNK11TbTimeStampgeERKS_   # TbTimeStamp::operator>=(TbTimeStamp const&) const
  __ZNK11TbTimeStampgtERKS_   # TbTimeStamp::operator>(TbTimeStamp const&) const
  __ZNK11TbTimeStampleERKS_   # TbTimeStamp::operator<=(TbTimeStamp const&) const
  __ZNK11TbTimeStampltERKS_   # TbTimeStamp::operator<(TbTimeStamp const&) const
  __ZNK11TbTimeStampmiERK10TbTimeDiff   # TbTimeStamp::operator-(TbTimeDiff const&) const
  __ZNK11TbTimeStampmiERKS_   # TbTimeStamp::operator-(TbTimeStamp const&) const
  __ZNK11TbTimeStampneERKS_   # TbTimeStamp::operator!=(TbTimeStamp const&) const
  __ZNK11TbTimeStampplERK10TbTimeDiff   # TbTimeStamp::operator+(TbTimeDiff const&) const
  __ZNK3bfl8compress5CZLib11MemCompressEPhPKhjS2_   # bfl::compress::CZLib::MemCompress(unsigned char*, unsigned char const*, unsigned int, unsigned char*) const
  __ZNK3bfl8compress5CZLib13MemDecompressEPhS2_S2_   # bfl::compress::CZLib::MemDecompress(unsigned char*, unsigned char*, unsigned char*) const
  __ZNK3bfl8compress5CZLib16BufferedCompressEPKhjPhjS4_RNS0_15CBufferCallbackE   # bfl::compress::CZLib::BufferedCompress(unsigned char const*, unsigned int, unsigned char*, unsigned int, unsigned char*, bfl::compress::CBufferCallback&) const
  __ZNK3bfl8compress5CZLib18BufferedDecompressEPhS2_jS2_RNS0_15CBufferCallbackE   # bfl::compress::CZLib::BufferedDecompress(unsigned char*, unsigned char*, unsigned int, unsigned char*, bfl::compress::CBufferCallback&) const
  __ZNK3bfl8compress5CZLib21CompressWorkspaceSizeEv   # bfl::compress::CZLib::CompressWorkspaceSize() const
  __ZNK3bfl8compress5CZLib23DecompressWorkspaceSizeEv   # bfl::compress::CZLib::DecompressWorkspaceSize() const
)
set(TPW_UNIT_StampZLib_OWN_CALLLOG ON)
cmake_language(DEFER CALL target_link_libraries tpw_unit_StampZLib PRIVATE ZLIB::ZLIB)
