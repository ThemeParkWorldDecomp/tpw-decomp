# Unit v5312: CStaticMBBuffer AppendMB x2, AppendUnicode x2, Close (src/ltms/StaticUnicodeBuffer.cpp). The callees outside
# the unit are recording fakes in tests/unit_CStaticMBBuffer_AppendMB.cpp.
set(TPW_UNIT_CStaticMBBuffer_AppendMB src/ltms/StaticUnicodeBuffer.cpp platform/arena.cpp)
set(TPW_UNIT_CStaticMBBuffer_AppendMB_KEEP
  __ZN15CStaticMBBuffer13AppendUnicodeEDs   # CStaticMBBuffer::AppendUnicode(char16_t)
  __ZN15CStaticMBBuffer13AppendUnicodeEPKDsj   # CStaticMBBuffer::AppendUnicode(char16_t const*, unsigned int)
  __ZN15CStaticMBBuffer5CloseEv   # CStaticMBBuffer::Close()
  __ZN15CStaticMBBuffer8AppendMBEPKhj   # CStaticMBBuffer::AppendMB(unsigned char const*, unsigned int)
  __ZN15CStaticMBBuffer8AppendMBEt   # CStaticMBBuffer::AppendMB(unsigned short)
)
