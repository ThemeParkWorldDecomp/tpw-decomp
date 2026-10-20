// 0x100ad6dc PostGetCityListCallbackFunction (the online globe's city list callback)
#include "lobby.h"
#include "FS_Online.h"
#include "TbTList.h"

void PostGetCityListCallbackFunction(int error, const void* context, TbPtrList<CFSOnlineCity>* cities) {
    CLBCityListScreen* screen = static_cast<CLBCityListScreen*>(const_cast<void*>(context));
    if (error == 0)
        screen->CityListReceived(cities);
    else
        screen->CityListFailed();
}
