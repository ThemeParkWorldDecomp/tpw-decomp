# Unit v51128: ltms destructors and CCharacterHandler (src/ltms/StaticUnicodeBuffer.cpp), CFile::Read (header inline).
set(TPW_UNIT_TbIMBInfo_Dtor_TbIMBInfo src/ltms/StaticUnicodeBuffer.cpp platform/arena.cpp
    tests/standins/TbIMBInfo_Dtor_TbIMBInfo.cpp)
set(TPW_UNIT_TbIMBInfo_Dtor_TbIMBInfo_KEEP
  __ZN15CStaticMBBufferD1Ev   # CStaticMBBuffer::~CStaticMBBuffer()
  __ZN17CCharacterHandlerC1Ev   # CCharacterHandler::CCharacterHandler()
  __ZN17CCharacterHandlerD1Ev   # CCharacterHandler::~CCharacterHandler()
)
