set(TPW_UNIT_TbITable src/ltms/TbITable.cpp src/ltms/TbIUnicodeToMB.cpp src/ltms/StaticUnicodeBuffer.cpp
    src/ltms/TbFontRenderParams.cpp src/ltms/TbIResourceIndex.cpp)
set(TPW_UNIT_TbITable_KEEP
  __ZN14TbIMBToUnicode13ConvertStringEPDsPKht   # TbIMBToUnicode::ConvertString(char16_t*, unsigned char const*, unsigned short)
  __ZN14TbIMBToUnicodeD1Ev   # TbIMBToUnicode::~TbIMBToUnicode()
  __ZN14TbIUnicodeToMB11LocateValueEDs   # TbIUnicodeToMB::LocateValue(char16_t)
  __ZN14TbIUnicodeToMB13ConvertStringEPhPKDst   # TbIUnicodeToMB::ConvertString(unsigned char*, char16_t const*, unsigned short)
  __ZN14TbIUnicodeToMBD1Ev   # TbIUnicodeToMB::~TbIUnicodeToMB()
  __ZN9TbIMBInfoD1Ev   # TbIMBInfo::~TbIMBInfo()
  __ZNK8TbITable12FormatStringEPDsjjz   # TbITable::FormatString(char16_t*, unsigned int, unsigned int, ...) const
  __ZNK8TbITable14GetConstStringEj   # TbITable::GetConstString(unsigned int) const
  __ZNK8TbITable15FindVariableTagEjRPc   # TbITable::FindVariableTag(unsigned int, char*&) const
  __ZNK8TbITable8DoFormatER13CStaticBufferjPc   # TbITable::DoFormat(CStaticBuffer&, unsigned int, char*) const
  __ZNK9TbIMBInfo8GetIndexERPKh   # TbIMBInfo::GetIndex(unsigned char const*&) const
)
