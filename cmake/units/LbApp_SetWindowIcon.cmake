# Unit v51127: small bullfrog functions (src/bullfrog/AndyGLL.cpp, src/bullfrog/Colour.cpp, src/bullfrog/Draw.cpp, src/bullfrog/LbApp.cpp, src/bullfrog/LbSCBack.cpp, src/bullfrog/LbZLib.cpp, src/bullfrog/TbJPEGSaver.cpp). The callees outside the unit are
# recording fakes in tests/unit_LbApp_SetWindowIcon.cpp, which also defines the globals of other files.
set(TPW_UNIT_LbApp_SetWindowIcon src/bullfrog/AndyGLL.cpp src/bullfrog/Colour.cpp src/bullfrog/Draw.cpp
    src/bullfrog/LbApp.cpp src/bullfrog/LbSCBack.cpp src/bullfrog/LbZLib.cpp src/bullfrog/TbJPEGSaver.cpp
    platform/arena.cpp)
set(TPW_UNIT_LbApp_SetWindowIcon_KEEP
  __Z13LbApp_GetHWndv   # LbApp_GetHWnd()
  __Z13LbApp_SetHWndPv   # LbApp_SetHWnd(void*)
  __Z17TbPoint_ConstructP7TbPointii   # TbPoint_Construct(TbPoint*, int, int)
  __Z18LbApp_GetHInstancev   # LbApp_GetHInstance()
  __Z18LbApp_SetHInstancePv   # LbApp_SetHInstance(void*)
  __Z19CConverter_DtorBodyP10CConverters   # CConverter_DtorBody(CConverter*, short)
  __Z19LbApp_SetWindowIcons   # LbApp_SetWindowIcon(short)
  __Z19_LbSemaLock_ReleaseP11_LbSemaLock   # _LbSemaLock_Release(_LbSemaLock*)
  __Z20LbApp_GetUserWndProcv   # LbApp_GetUserWndProc()
  __Z20LbApp_SetUserWndProcPv   # LbApp_SetUserWndProc(void*)
  __Z21TbJPEGLoader_BaseCtorPv   # TbJPEGLoader_BaseCtor(void*)
  __Z21TbJPEGLoader_BaseDtorPvs   # TbJPEGLoader_BaseDtor(void*, short)
  __Z21TbJPEGLoader_CtorBodyP12TbJPEGLoader   # TbJPEGLoader_CtorBody(TbJPEGLoader*)
  __Z21TbJPEGLoader_DtorBodyP12TbJPEGLoaders   # TbJPEGLoader_DtorBody(TbJPEGLoader*, short)
  __Z21_LbSemaLock_ConstructP11_LbSemaLockP12_LbSemaphorei   # _LbSemaLock_Construct(_LbSemaLock*, _LbSemaphore*, int)
  __Z25LbApp_GetCloseDownRequestv   # LbApp_GetCloseDownRequest()
  __Z25LbApp_SetCloseDownRequesti   # LbApp_SetCloseDownRequest(int)
  __Z27TbPtrList_LbSEFunction_CtorP9TbPtrListI13_LbSEFunctionE   # TbPtrList_LbSEFunction_Ctor(TbPtrList<_LbSEFunction>*)
  __Z28TbTListLbSEFunction_DtorBodyP7TbTListIP13_LbSEFunctionS1_Es   # TbTListLbSEFunction_DtorBody(TbTList<_LbSEFunction*, _LbSEFunction*>*, short)
  __Z31TbPtrList_LbSEFunction_DtorBodyP9TbPtrListI13_LbSEFunctionEs   # TbPtrList_LbSEFunction_DtorBody(TbPtrList<_LbSEFunction>*, short)
  __ZN13SamsUtilities27USwapBlock32_ulong_10009ce4EPji   # SamsUtilities::USwapBlock32_ulong_10009ce4(unsigned int*, int)
  __ZN13SamsUtilities27USwapBlock32_ulong_10044590EPji   # SamsUtilities::USwapBlock32_ulong_10044590(unsigned int*, int)
  __ZN13SamsUtilities29USwapInplace32_ulong_10009d44ERj   # SamsUtilities::USwapInplace32_ulong_10009d44(unsigned int&)
  __ZN13SamsUtilities29USwapInplace32_ulong_100445f0ERj   # SamsUtilities::USwapInplace32_ulong_100445f0(unsigned int&)
  __ZN13SamsUtilities7UBinary16RotateLeft_ulongEji   # SamsUtilities::UBinary::RotateLeft_ulong(unsigned int, int)
  __ZN7TbTListIP13_LbSEFunctionS1_E10RemoveHeadEv   # TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveHead()
  __ZN7TbTListIP13_LbSEFunctionS1_E9RemoveAllEv   # TbTList<_LbSEFunction*, _LbSEFunction*>::RemoveAll()
  __ZN7TbTListIP13_LbSEFunctionS1_EC1Ev   # TbTList<_LbSEFunction*, _LbSEFunction*>::TbTList()
  __ZN7TbTListIP13_LbSEFunctionS1_ED1Ev   # TbTList<_LbSEFunction*, _LbSEFunction*>::~TbTList()
  _gLbAppCloseDown
  _gLbAppHInstance
  _gLbAppHWnd
  _gLbAppUserWndProc
)
