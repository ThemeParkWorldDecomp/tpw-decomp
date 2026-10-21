# Unit w217: bullfrog date formatter; calendar imports and string virtuals are recording fakes.
set(TPW_UNIT_StampFormat src/bullfrog/TbTimeStamp.cpp)
set(TPW_UNIT_StampFormat_KEEP
  __ZNK11TbTimeStamp6FormatEP22TbStaticStringTemplateIDsEPKDs   # TbTimeStamp::Format(TbStaticStringTemplate<char16_t>*, char16_t const*) const
  __ZNK11TbTimeStamp6FormatEP22TbStaticStringTemplateIcEPKc   # TbTimeStamp::Format(TbStaticStringTemplate<char>*, char const*) const
  __ZNK11TbTimeStamp6FormatEP23TbDynamicStringTemplateIDsEPKDs   # TbTimeStamp::Format(TbDynamicStringTemplate<char16_t>*, char16_t const*) const
  __ZNK11TbTimeStamp6FormatEP23TbDynamicStringTemplateIcEPKc   # TbTimeStamp::Format(TbDynamicStringTemplate<char>*, char const*) const
  __ZNK11TbTimeStamp6FormatEPciPKc   # TbTimeStamp::Format(char*, int, char const*) const
  __Z16ProcessFormatTagIDsEvPPT_PiPPKS0_N10NS_MacDoze11_SYSTEMTIMEE   # template void ProcessFormatTag(char16_t**, int32_t*, const char16_t**, NS_MacDoze::_SYSTEMTIME) (explicit instantiation)
  __Z16ProcessFormatTagIcEvPPT_PiPPKS0_N10NS_MacDoze11_SYSTEMTIMEE   # template void ProcessFormatTag(char**, int32_t*, const char**, NS_MacDoze::_SYSTEMTIME) (explicit instantiation)
  __Z21TemplatedStaticFormatI22TbStaticStringTemplateIDsEDsEiPT_PKT0_PK11TbTimeStamp   # template int32_t TemplatedStaticFormat(TbStaticStringTemplate<char16_t>*, const char16_t*, const TbTimeStamp*) (explicit instantiation)
  __Z21TemplatedStaticFormatI22TbStaticStringTemplateIcEcEiPT_PKT0_PK11TbTimeStamp   # template int32_t TemplatedStaticFormat(TbStaticStringTemplate<char>*, const char*, const TbTimeStamp*) (explicit instantiation)
  __Z22TemplatedDynamicFormatI23TbDynamicStringTemplateIDsEDsEiPT_PKT0_PK11TbTimeStamp   # template int32_t TemplatedDynamicFormat(TbDynamicStringTemplate<char16_t>*, const char16_t*, const TbTimeStamp*) (explicit instantiation)
  __Z22TemplatedDynamicFormatI23TbDynamicStringTemplateIcEcEiPT_PKT0_PK11TbTimeStamp   # template int32_t TemplatedDynamicFormat(TbDynamicStringTemplate<char>*, const char*, const TbTimeStamp*) (explicit instantiation)
  tmpl_1a714ba7225a@bullfrog/TbTimeStamp.cpp   # TemplatedDynamicFormat (a template definition src/TbTimeStamp_format.cpp had before the regroup)
  tmpl_765ae691fd8f@bullfrog/TbTimeStamp.cpp   # ProcessFormatTag (a template definition src/TbTimeStamp_format.cpp had before the regroup)
  tmpl_dee986343647@bullfrog/TbTimeStamp.cpp   # TemplatedStaticFormat (a template definition src/TbTimeStamp_format.cpp had before the regroup)
)
