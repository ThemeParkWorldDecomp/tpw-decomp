#include "Debug.h"
#include "FS_Online.h"
#include "TPMain.h"
#include "FS_MasterSave.h"

#include <new>

// 0x1016f080: std::pair<const TbDynamicStringTemplate<char>, CFSGlobalParkData*>::pair(const pair&).
// Copy-constructs the key (0x10238308), then copies the record pointer (Mac offset 0x14). The original returns this; the declaration is void.
void ParkThemeTreeCallee::PairCopy(ParkThemeTree::Pair* pair, const ParkThemeTree::Pair& value) {
    new (const_cast<ParkThemeTree::Key*>(&pair->first)) ParkThemeTree::Key(value.first);
    pair->second = value.second;
}
