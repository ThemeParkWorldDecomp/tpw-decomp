# Unit v5L16: UIMain_GetObjectName and CalcZTrans (src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/ui_main.cpp). CObject::GetUIName is a recording fake in tests/unit_TailSeven.cpp,
# which also defines gUIMainConvertBuffer.
set(TPW_UNIT_TailSeven src/game/ui/InterfaceDrawEngine.cpp src/game/ui/ui_main.cpp)
set(TPW_UNIT_TailSeven_KEEP
  __Z10CalcZTransi   # CalcZTrans(int)
  __Z20UIMain_GetObjectNameP7CObject   # UIMain_GetObjectName(CObject*)
)
