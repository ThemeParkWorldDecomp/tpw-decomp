# Unit v5138: Argument::Argument and Argument::~Argument (src/game/online/email.cpp). The callees are recording fakes in
# tests/unit_Argument_Argument.cpp.
set(TPW_UNIT_Argument_Argument src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_Argument_Argument_KEEP
  __ZN8Argument8DestructEPS_s   # Argument::Destruct(Argument*, short)
  __ZN8Argument9ConstructEPS_PKv   # Argument::Construct(Argument*, void const*)
)
