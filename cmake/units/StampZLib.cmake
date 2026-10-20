# Unit w216: stamps, packing and buffered unpacking with the real host zlib.
set(TPW_UNIT_StampZLib src/TbTimeStamp_calendar.cpp src/TbTimeStamp_ops.cpp src/lb_zlib_compress.cpp src/lb_zlib.cpp)
set(TPW_UNIT_StampZLib_OWN_CALLLOG ON)
cmake_language(DEFER CALL target_link_libraries tpw_unit_StampZLib PRIVATE ZLIB::ZLIB)
