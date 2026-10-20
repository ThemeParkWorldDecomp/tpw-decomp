set(TPW_UNIT_bflAccess src/bfl_FileSystem.cpp src/bfl_DiscAccess.cpp src/bfl_Access.cpp
    src/bfl_WadAccess.cpp src/bfl_Refpack.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
# The unit file includes calllog.cpp itself.
set(TPW_UNIT_bflAccess_OWN_CALLLOG ON)
