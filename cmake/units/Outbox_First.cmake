# Unit v5950: Outbox::First, Next, MarkToSend, MarkToDelete, Delete, std::operator== on strings, the basic_ifstream<char> constructor and
# Email::Attachment::~Attachment (src/email_load_Outbox_Send_Outbox_WriteMailData_Outbox_First.cpp). The callees outside the unit are recording
# fakes in tests/unit_Outbox_First.cpp, which also defines the globals of other files.
set(TPW_UNIT_Outbox_First src/email_load_Outbox_Send_Outbox_WriteMailData_Outbox_First.cpp platform/arena.cpp)
