// Harness for CHField::AdjustHField (src/engine/tb_hfield.cpp). The function calls nothing,
// so there are no recording fakes; the constructor and destructor are empty stand-ins
// because the test only needs the object's storage.
#include "tb_hfield.h"
#include "m3d2.h"
#include <cstring>

CHField::CHField() {}
CHField::~CHField() {}

namespace {
CHField field;   // static: the object is too large for a comfortable stack frame
sHField record;
unsigned char fieldBefore[sizeof(CHField)];
unsigned char recordBefore[sizeof(sHField)];
}

// Runs AdjustHField on an object filled with 0xa5 whose record has the given counts and
// whose height copy starts `before` words into `words` (changed in place). Returns bit 0
// when the object's bytes are unchanged afterwards and bit 1 when the record's are.
extern "C" int32_t tpw_ha_run(uint32_t countX, uint32_t countZ, uint32_t* words, int32_t before) {
    std::memset(static_cast<void*>(&field), 0xa5, sizeof field);
    std::memset(&record, 0xa5, sizeof record);
    record.mF18[0] = countX;
    record.mF18[1] = countZ;
    field.mF00 = &record;
    field.mF12970 = words + before;
    std::memcpy(fieldBefore, &field, sizeof field);
    std::memcpy(recordBefore, &record, sizeof record);
    field.AdjustHField();
    const int32_t fieldSame = std::memcmp(fieldBefore, &field, sizeof field) == 0;
    const int32_t recordSame = std::memcmp(recordBefore, &record, sizeof record) == 0;
    return fieldSame | recordSame << 1;
}
