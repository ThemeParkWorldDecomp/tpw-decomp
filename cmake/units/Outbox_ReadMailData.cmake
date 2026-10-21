# Unit v5148: Outbox::ReadMailData (src/game/online/email.cpp). The callees outside the unit are recording fakes in
# tests/unit_Outbox_ReadMailData.cpp, which also holds the Outbox object the test drives.
set(TPW_UNIT_Outbox_ReadMailData src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_Outbox_ReadMailData_KEEP
  __ZN13outbox_callee10StreamCtorEP10SaveStream   # outbox_callee::StreamCtor(SaveStream*)
  __ZN13outbox_callee10StreamOpenEP10SaveStreamPKc   # outbox_callee::StreamOpen(SaveStream*, char const*)
  __ZN13outbox_callee11StreamWidenEP10SaveStream   # outbox_callee::StreamWiden(SaveStream*)
  __ZN13outbox_callee13StreamDestroyEP10SaveStream   # outbox_callee::StreamDestroy(SaveStream*)
  __ZN13outbox_callee13StreamGetlineEP10SaveStreamPcic   # outbox_callee::StreamGetline(SaveStream*, char*, int, char)
  __ZN6Outbox12ReadMailDataEi   # Outbox::ReadMailData(int)
)
