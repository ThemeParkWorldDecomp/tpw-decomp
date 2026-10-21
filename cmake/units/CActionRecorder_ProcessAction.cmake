# Unit v5254: CActionRecorder::ProcessAction overloads. The callees are recording fakes in tests/unit_CActionRecorder_ProcessAction.cpp.
set(TPW_UNIT_CActionRecorder_ProcessAction src/game/ActionRecorder.cpp platform/arena.cpp)
set(TPW_UNIT_CActionRecorder_ProcessAction_KEEP
  __ZN15CActionRecorder13ProcessActionEi   # CActionRecorder::ProcessAction(int)
  __ZN15CActionRecorder13ProcessActionEifft   # CActionRecorder::ProcessAction(int, float, float, unsigned short)
  __ZN15CActionRecorder13ProcessActionEift   # CActionRecorder::ProcessAction(int, float, unsigned short)
  __ZN15CActionRecorder13ProcessActionEihh   # CActionRecorder::ProcessAction(int, unsigned char, unsigned char)
  __ZN15CActionRecorder13ProcessActionEihht   # CActionRecorder::ProcessAction(int, unsigned char, unsigned char, unsigned short)
  __ZN15CActionRecorder13ProcessActionEith   # CActionRecorder::ProcessAction(int, unsigned short, unsigned char)
  __ZN15CActionRecorder13ProcessActionEitt   # CActionRecorder::ProcessAction(int, unsigned short, unsigned short)
  __ZN15CActionRecorder13ProcessActionEittt   # CActionRecorder::ProcessAction(int, unsigned short, unsigned short, unsigned short)
)
