// The map's core unit plus a call of CTrackCell::ModifyTypeBody, so a difftest can read the answer that decides whether
// CTrackCell::Modify stores the word the map and track strokes share (Mac 0x102dc21c).
#define private public
#define protected public
#include "unit_MapCells.cpp"
#undef private
#undef protected

extern "C" int32_t tpw_mc_modify_answer(int32_t mode) {
    return reinterpret_cast<CTrackCell*>(gTrackBlock + 0x20)->ModifyTypeBody(static_cast<CELL_MODES>(mode));
}
