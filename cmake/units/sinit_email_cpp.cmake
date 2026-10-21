# Unit v5567: email.cpp's static initialiser (src/game/online/email.cpp). The callees outside the unit are recording fakes in
# tests/unit_sinit_email_cpp.cpp.
set(TPW_UNIT_sinit_email_cpp src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_sinit_email_cpp_KEEP
  __Z16EmailOperatorNewm   # EmailOperatorNew(unsigned long)
  __Z17__sinit_email_cppv   # __sinit_email_cpp()
  _gEmailAppNameString
  _gEmailBlob
  _gEmailBlobDtorVec
  _gEmailBlobNode
  _gEmailDefaultByte
  _gEmailLanguageKeyString
  _gEmailLanguageStrings
  _gEmailNodes
  _gEmailWideString
)
