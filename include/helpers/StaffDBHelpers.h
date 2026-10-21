// StaffDBHelpers.h: file-local helpers that src/game/people/StaffDB.cpp shares with StaffInfo.cpp. Before the regroup,
// the functions using them shared a source file (1 file), which had one copy of each; that copy now lives here, static
// inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "TbITable.h"
#include "Text.h"
#include "TimeSig.h"

constexpr const char* kBadName = "Trying to get the name of a non-existent staffType!!!\n";

static inline bool ValidType(uint32_t type) { return static_cast<int32_t>(type) >= 0 && static_cast<int32_t>(type) <= 4; }

static inline void FormatName(const CStaffInfo* record, char16_t* buffer, int32_t size) {
    if (ValidType(record->mType)) {
        static_cast<TbITable*>(gText.mResourcePointers[11 + record->mType])->FormatString(buffer, size, record->mName);
    } else {
        TRACE(kBadName);
    }
}

template<class T> static inline int32_t Field(void* file, int32_t save, T* value, const char* name) {
    return save != 0 ? SAVEVAR(file, value, name, 0) : LOADVAR(file, value, name, 0);
}
