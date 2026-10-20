// TPSettings.cpp range of the main app (SimTheme Park 2.04b3): the constructor of the ride database's item list resource
// (0x10161148, 280 bytes). The rest of the class (destructor 0x101621f4, RegisterItem, FindItem, OpenFile) is not translated.
#include "TPResource.h"

// The original builds the base resource inline (the two strings and the empty list of storages, as CTPBaseResource's
// constructor does), then the texture resource's file storage at 0x40 (CTPTextureResource), and clears the two words it adds
// at 0x48 and 0x4c. The vtable word is written four times on the Mac while the parts are built (the base resource's, the texture
// resource's, the intermediate's, the final one); the host's chain of constructors does the same.
CTPItemListResource::CTPItemListResource(const TbDynamicStringTemplate<char>& name, const TbDynamicStringTemplate<char>& path)
    : CTPIntermediate(name, path), mF48(nullptr), mItems(nullptr) {}

// ~CTPItemListResource (0x101621f4) is in src/TPSettings_itemres_CTPItemListResource_OpenFile.cpp (unit v5375).
