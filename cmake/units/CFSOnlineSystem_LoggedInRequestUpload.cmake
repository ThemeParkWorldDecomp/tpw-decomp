# Unit v5043: the upload job and the city unpublish request (src/FS_Online_init_CFSOnlineSystem_LoggedInRequestUpload.cpp). The callees outside the
# unit are recording fakes in tests/unit_CFSOnlineSystem_LoggedInRequestUpload.cpp, which also defines the globals of other files and the
# operator new / delete (so the arena, which defines them, is not linked).
set(TPW_UNIT_CFSOnlineSystem_LoggedInRequestUpload src/FS_Online_init_CFSOnlineSystem_LoggedInRequestUpload.cpp)
