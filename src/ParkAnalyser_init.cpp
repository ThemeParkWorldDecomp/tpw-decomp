// CParkAnalyser::Init, main app 0x100e4a44 (456 bytes; analyser.cpp), unit w123b.
#include <cstddef>
#include <new>

#include "CMessageCentre.h"
#include "World.h"
#include "analyser.h"

namespace {
// The lazy centre block at 0x100e4a64 and twice more (repeated before EACH subscription, as in CChallengeHQ::CChallengeHQ): when the global is NULL a
// centre is allocated (operator new answering NULL is tolerated) and constructed, and the result, NULL included, is stored back. The caller reads the
// global again, so a Subscribe that cleared it makes the next block allocate again; Subscribe is called on whatever the global holds.
CMessageCentre* MessageCentreForSubscribe() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// The 32 bit wrapping arithmetic of the original (mullw, add, subf).
int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }
int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t Sub(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
}  // namespace

// Subscribes to the messages 0xc, 0x11 and 0x13 (answers ignored), then gives each of the grid's cells its place: the loop limit is the low 32 bits of
// the signed product of the grid's two sizes, taken once; the grid width, the squares per cell and the map sizes are read again in every iteration.
// col = i - (i / gridX) * gridX and row = i / gridX (signed divides). A cell starts at squares * col and squares * row and is squares wide and high,
// except that a cell reaching past the map (signed squares * (col + 1) > mapSizeX, and likewise for rows, strict) is cut to (mapSize + squares) -
// squares * (col + 1). CParkAnalyserCell::Init (a stand-in) stores the four numbers; its answer is ignored. Answers 1 on every path, including a
// limit that is zero or negative. The cells are addressed as this + 0x20 + i * 0x7c without a bounds test (a grid larger than 1024 cells would run
// past the array, as the original does).
int32_t CParkAnalyser::Init() {
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0xc));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x11));
    MessageCentreForSubscribe()->Subscribe(mBase, static_cast<MsgType>(0x13));
    const int32_t limit = Mul(mGridSizeX, mGridSizeY);
    for (int32_t i = 0; i < limit; ++i) {
        const int32_t gridX = mGridSizeX;
        const int32_t squares = mMapSquaresPerCell;
        const int32_t row = i / gridX;
        const int32_t col = Sub(i, Mul(row, gridX));
        const int32_t right = Mul(squares, Add(col, 1));
        int32_t width = squares;
        if (right > mMapSizeX) width = Sub(Add(mMapSizeX, squares), right);
        const int32_t bottom = Mul(squares, Add(row, 1));
        int32_t height = squares;
        if (bottom > mMapSizeY) height = Sub(Add(mMapSizeY, squares), bottom);
        auto* cell = reinterpret_cast<CParkAnalyserCell*>(reinterpret_cast<unsigned char*>(this) + 0x20 + static_cast<std::ptrdiff_t>(i) * 0x7c);
        cell->Init(Mul(squares, col), Mul(squares, row), width, height);
    }
    return 1;
}
