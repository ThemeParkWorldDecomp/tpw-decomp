# Unit v5963: RegisterBullfrogWindowClass, _ScreenFPUHandler::~_ScreenFPUHandler, __sinit_TbSystemInfo_cpp, LbRect_Contains,
# LbRect_Bounding, CRefpack::MemCompress (src/bullfrog/FileSystem.cpp, src/bullfrog/LbRect.cpp, src/bullfrog/TbSystemInfo.cpp, src/bullfrog/WinScrn.cpp). The callees outside the
# unit are recording fakes in tests/unit_RegisterBullfrogWindowClass_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_RegisterBullfrogWindowClass_2 src/bullfrog/FileSystem.cpp src/bullfrog/LbRect.cpp
    src/bullfrog/TbSystemInfo.cpp src/bullfrog/WinScrn.cpp platform/arena.cpp)
set(TPW_UNIT_RegisterBullfrogWindowClass_2_KEEP
  __Z15LbRect_BoundingP6TbRectPKS_   # LbRect_Bounding(TbRect*, TbRect const*)
  __Z15LbRect_ContainsPK6TbRectS1_   # LbRect_Contains(TbRect const*, TbRect const*)
  __Z16Refpack_NewArrayj   # Refpack_NewArray(unsigned int)
  __Z19Refpack_DeleteArrayPv   # Refpack_DeleteArray(void*)
  __Z21ScreenFPUHandler_DtorP17_ScreenFPUHandlers   # ScreenFPUHandler_Dtor(_ScreenFPUHandler*, short)
  __Z24__sinit_TbSystemInfo_cppv   # __sinit_TbSystemInfo_cpp()
  __Z27RegisterBullfrogWindowClassv   # RegisterBullfrogWindowClass()
  __Z31ScreenFPUHandler_OperatorDeletePv   # ScreenFPUHandler_OperatorDelete(void*)
  __ZNK3bfl8compress8CRefpack11MemCompressEPhS2_jS2_   # bfl::compress::CRefpack::MemCompress(unsigned char*, unsigned char*, unsigned int, unsigned char*) const
  _gRegisterBullfrogWindowClassDone
  _gRegisterBullfrogWindowClassGuard
  _gTbSystemInfoCPU
  _gTbSystemInfoF1C
  _kScreenFPUHandlerFlagVtable
)
