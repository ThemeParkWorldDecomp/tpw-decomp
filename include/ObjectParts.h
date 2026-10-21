// Small classes CObject::CObject reaches (unit w126, main app). Both are untranslated: only the members the constructor calls are declared,
// and the launcher gets a stand-in for each.
#pragma once
#include <cstdint>

struct IDXTYPEUniqueNameObjectId;
template<class Tag, class T> struct tag_IDX_TYPE;

// A map offset the ride record keeps three times (the map offset at +0x7ac, the entry at +0x790 and the exit at +0x79c of the ride record): an x, a y and
// a flag byte (12 bytes on the Mac; the record's own members hold them back to back). Rotate answers the triple turned for a ride rotated by
// `rotation`, through a hidden pointer. The constructor of CObject reads x and y.
struct MapDelta {
    int32_t mX;
    int32_t mY;
    uint8_t mFlag;
    // 0x10103d88 (untranslated): the offset turned for a ride rotated by `rotation`.
    MapDelta Rotate(int32_t rotation) const;
    // 0x101037d0 (28 bytes, src/game/park/MapDelta.cpp, unit w145): both words times k (hidden result; the original never writes the flag byte, the host writes 0).
    MapDelta operator*(int32_t k) const;
};

// Unit w139 (src/game/park/MapDelta.cpp): MapDelta::Rotate is translated (0x10103d88, 272 bytes). The three compass offsets CTrackCell::ModifyType hands to
// GetNeighbouringCell (Mac 0x1034c284, 0x1034c270, 0x1034c25c; the names are mine).
extern const MapDelta gMapDeltaPlusX;        // (1, 0)
extern const MapDelta gMapDeltaPlusXPlusY;   // (1, 1)
extern const MapDelta gMapDeltaPlusY;        // (0, 1)

// The golden ticket kinds (full definitions in FS_MasterSave.h: opaque here so this header does not include it) and the answer of
// CFSGlobalMasterSave::EarnedTicket that TellTheWorld takes (mangled as the by-value struct 19GoldenTicketTypeWon): the values are
// EarnedTicket's answers, 1 to 3 (see FS_MasterSave.h 140-147; the global one can answer 4). Names invented.
enum GoldenTicketTypeLocal : int32_t;
enum GoldenTicketTypeGlobal : int32_t;
enum GoldenTicketTypeSecret : int32_t;
enum GoldenTicketTypeWon : int32_t {};

// The golden ticket control the world keeps at CWorld + 0x2c0 (the end of the park time's words, as World.h lays them out; reached only by
// address arithmetic). The tickets are spent per ride type.
class CGoldenTicketControl {
public:
    // 0x100fbfec (116 bytes, src/game/park/GoldenTickets.cpp, unit w180): whether a golden ticket was already spent on the ride type.
    bool HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const;
    // 0x100fbea0 (228 bytes, src/game/park/GoldenTickets.cpp, unit w180): spends a golden ticket on the ride type; the Mac answers r3 (true when it was
    // spent, or at once, with nothing spent, in the online game type 1).
    bool SpendTicketOn(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type);

    // Unit w129 (src/game/park/GoldenTickets.cpp). 0x100fbd2c (144 bytes): makes the game type object on first use (the guard byte at 0x102b3aa4), then, for
    // the normal game (type 0) only, checks the local, global and secret tickets in that order.
    void CheckForTickets();
    void CheckForLocal();    // 0x100fc1d0 (864 bytes, src/game/park/GoldenTickets.cpp, unit w180)
    void CheckForGlobal();   // 0x100fc56c (284 bytes, src/game/park/GoldenTickets.cpp, unit w180)
    void CheckForSecret();   // 0x100fc6c4 (196 bytes, src/game/park/GoldenTickets.cpp, unit w180)

    // Unit w180 (src/game/park/GoldenTickets.cpp).
    int32_t HowManyTickets() const;                                                          // 0x100fbdf8 (108 bytes)
    bool CanIAfford(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const;           // 0x100fc0c8 (160 bytes)
    // The three EarnedGoldenTicket answer 1 when the ticket was won now (and the world was told, and the total traced), 0 otherwise.
    bool EarnedGoldenTicket(GoldenTicketTypeLocal type);                                     // 0x100fc7c4 (252 bytes)
    bool EarnedGoldenTicket(GoldenTicketTypeGlobal type, int32_t score);                     // 0x100fc918 (236 bytes)
    bool EarnedGoldenTicket(GoldenTicketTypeSecret type);                                    // 0x100fca5c (252 bytes)
    // Unit w183 (src/game/park/GoldenTickets.cpp): announces a won ticket (the return type is a guess: the callers ignore r3). `type` is the ticket's number within
    // its group, `won` what EarnedTicket answered (1 ticket, 2 and key, 3 and park; the global one also 4, the award).
    void TellTheWorld(GoldenTicketTypeLocal type, GoldenTicketTypeWon won);                  // 0x100fcbb0 (660 bytes)
    void TellTheWorld(GoldenTicketTypeGlobal type, GoldenTicketTypeWon won);                 // 0x100fcf1c (1392 bytes)
    void TellTheWorld(GoldenTicketTypeSecret type, GoldenTicketTypeWon won);                 // 0x100fd4f4 (660 bytes)
    // 0x100fd7f0 (420 bytes): when the player holds exactly one ticket, the advisor says two things (messages 0x137 and 0x13b).
    void TellIfFirst();
};

// Unit w145 (src/game/park/Map.cpp): the other five compass offsets (Mac 0x1034c2ac, 0x1034c298, 0x1034c248, 0x1034c234, 0x1034c220; the names are mine),
// and the two direction helpers on the direction bytes (1 up, 2, 4 ... 0x80 around).
extern const MapDelta gMapDeltaMinusY;        // (0, -1)
extern const MapDelta gMapDeltaPlusXMinusY;   // (1, -1)
extern const MapDelta gMapDeltaMinusXPlusY;   // (-1, 1)
extern const MapDelta gMapDeltaMinusX;        // (-1, 0)
extern const MapDelta gMapDeltaMinusXMinusY;  // (-1, -1)
uint8_t OPPOSITE(uint8_t direction);                  // 0x10102500 (44 bytes)
uint8_t ADD_DIRECTION(uint8_t first, uint8_t second);   // 0x1010254c (80 bytes)
