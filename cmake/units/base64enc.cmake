# Unit v51015: base64enc, Email::Attachment::Attachment, Connection::Connection, Connection::SendCommandLine, CUIGrapher::DrawAxes
# (src/game/online/Connection.cpp, src/game/online/EmailMessage.cpp, src/game/ui/UIGrapher.cpp). The callees outside the unit are recording fakes in tests/unit_base64enc.cpp.
set(TPW_UNIT_base64enc src/game/online/Connection.cpp src/game/online/EmailMessage.cpp src/game/ui/UIGrapher.cpp
    platform/arena.cpp)
set(TPW_UNIT_base64enc_KEEP
  __Z9base64encPhS_i   # base64enc(unsigned char*, unsigned char*, int)
  __ZN10CUIGrapher8DrawAxesEv   # CUIGrapher::DrawAxes()
  __ZN10Connection15SendCommandLineEPc   # Connection::SendCommandLine(char*)
  __ZN10ConnectionC1Ev   # Connection::Connection()
  __ZN13attach_callee11DeleteArrayEPv   # attach_callee::DeleteArray(void*)
  __ZN13attach_callee5FopenEPKcS1_   # attach_callee::Fopen(char const*, char const*)
  __ZN13attach_callee5FreadEPvjjS0_   # attach_callee::Fread(void*, unsigned int, unsigned int, void*)
  __ZN13attach_callee5FseekEPvii   # attach_callee::Fseek(void*, int, int)
  __ZN13attach_callee5FtellEPv   # attach_callee::Ftell(void*)
  __ZN13attach_callee6FcloseEPv   # attach_callee::Fclose(void*)
  __ZN13attach_callee8NewArrayEj   # attach_callee::NewArray(unsigned int)
  __ZN5Email10Attachment9ConstructEPS0_PKc   # Email::Attachment::Construct(Email::Attachment*, char const*)
)
