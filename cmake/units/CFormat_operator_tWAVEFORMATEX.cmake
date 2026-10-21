# Unit v51124: small sound functions (CFormat operator, CFile::Read, TbError, CAutoArrayPtr<void*>; src/sound/TbSoundPipe.cpp).
# The callees outside the unit are recording fakes in tests/unit_CFormat_operator_tWAVEFORMATEX.cpp.
set(TPW_UNIT_CFormat_operator_tWAVEFORMATEX src/sound/TbSoundPipe.cpp platform/arena.cpp)
set(TPW_UNIT_CFormat_operator_tWAVEFORMATEX_KEEP
  __Z10CFile_ReadP14FileAccessReadPvj   # CFile_Read(FileAccessRead*, void*, unsigned int)
  __Z12TbError_ctorPii   # TbError_ctor(int*, int)
  __Z12VoidArrayNewm   # VoidArrayNew(unsigned long)
  __Z15VoidArrayDeletePv   # VoidArrayDelete(void*)
  __Z16VoidObjectDeletePv   # VoidObjectDelete(void*)
  __Z21TbError_operator_longPKi   # TbError_operator_long(int const*)
  __Z22CAutoArrayPtrVoid_ctorPPPvj   # CAutoArrayPtrVoid_ctor(void***, unsigned int)
  __Z22CAutoArrayPtrVoid_dtorPPPvs   # CAutoArrayPtrVoid_dtor(void***, short)
  __Z30CAutoArrayPtrVoid_operator_ptrPPPv   # CAutoArrayPtrVoid_operator_ptr(void***)
  __Z30CFormat_operator_tWAVEFORMATEXP7CFormat   # CFormat_operator_tWAVEFORMATEX(CFormat*)
)
