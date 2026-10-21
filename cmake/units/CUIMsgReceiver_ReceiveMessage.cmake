# Unit v5666: CUIMsgReceiver::ReceiveMessage (src/game/ui/vector2d.cpp). The UIInterface callees are recording fakes in
# tests/unit_CUIMsgReceiver_ReceiveMessage.cpp.
set(TPW_UNIT_CUIMsgReceiver_ReceiveMessage src/game/ui/vector2d.cpp platform/arena.cpp)
set(TPW_UNIT_CUIMsgReceiver_ReceiveMessage_KEEP
  __ZN14CUIMsgReceiver14ReceiveMessageERK4CMsg   # CUIMsgReceiver::ReceiveMessage(CMsg const&)
)
