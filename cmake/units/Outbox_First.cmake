# Unit v5950: Outbox::First, Next, MarkToSend, MarkToDelete, Delete, std::operator== on strings, the basic_ifstream<char> constructor and
# Email::Attachment::~Attachment (src/game/online/email.cpp). The callees outside the unit are recording
# fakes in tests/unit_Outbox_First.cpp, which also defines the globals of other files.
set(TPW_UNIT_Outbox_First src/game/online/email.cpp platform/arena.cpp)
set(TPW_UNIT_Outbox_First_KEEP
  __Z17BasicIfstreamCtorP11MacIfstreams   # BasicIfstreamCtor(MacIfstream*, short)
  __ZN15msl_char_string5equalEPK13MslCharStringS2_   # msl_char_string::equal(MslCharString const*, MslCharString const*)
  __ZN5Email10Attachment8DestructEPS0_s   # Email::Attachment::Destruct(Email::Attachment*, short)
  __ZN6Outbox10MarkToSendEv   # Outbox::MarkToSend()
  __ZN6Outbox12MarkToDeleteEv   # Outbox::MarkToDelete()
  __ZN6Outbox4NextEv   # Outbox::Next()
  __ZN6Outbox5FirstEv   # Outbox::First()
  __ZN6Outbox6DeleteEv   # Outbox::Delete()
)
