# Unit v5711: EmailInfo::~EmailInfo (src/game/online/email.cpp). The callees are recording fakes in
# tests/unit_EmailInfo_Dtor_EmailInfo.cpp.
set(TPW_UNIT_EmailInfo_Dtor_EmailInfo src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_EmailInfo_Dtor_EmailInfo_KEEP
  __ZN9EmailInfo8DestructEPS_s   # EmailInfo::Destruct(EmailInfo*, short)
)
