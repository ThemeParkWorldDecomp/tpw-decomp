// The analyser.cpp range of the inventory (main app 0x100d...-0x100e...): the park
// analyser, the scripts, the dynamic cell meshes and the white-out. The sound wrappers
// of the same range are in TPSound.h. PARTIAL; untranslated.
#pragma once
#include <cstdint>
#include <cstddef>
#include "History.h"
#include "ThingParts2.h"

class CMsg;
class CSerialise;   // unit w205: FS_MasterSave.h defines it (an empty tag)
struct s3DPoint;
struct MapPos;
struct IDXTYPEUniqueNameThingId;
struct IDXTYPEUniqueNameObjectId;
struct IDXTYPEUniqueNameMapId;
template<class Tag, class T> struct tag_IDX_TYPE;

// CThing: PARTIAL. The thing table's objects (CPerson, CObject, CAdvisor and the others are things). Only the first fields are known
// (unit w107): the id at +0 and the kind byte at +2, which ReceiveMessage switches on. Offsets are the Mac's.
class CThing {
public:
    enum ThingModel : int32_t {};
    // 0x1012e1f8 (12 bytes): the thing's own id, a copy of the half-word at +0 (a two-byte struct, returned through a hidden pointer).
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> GetThingId() const;
    // 0x1012e470 (272 bytes): hands the message to the ReceiveMessage of the thing's own class, by the kind byte; kinds 0, 2 and
    // above 0x13 do nothing. The classes are not CThing's subclasses in the host headers, so the thing's address is cast.
    void ReceiveMessage(const CMsg& message);

    uint16_t mThingId;   // 0x00
    uint8_t mKind;       // 0x02  1 CPerson, 3 CObject, 4 CMechanic ... 0x13 CChallengeHQ (see src/Thing_receive.cpp)
    uint8_t mDead;       // 0x03  1 once CThing::KillMe has run (it also makes the kind 2); the world's walks skip dead things (unit w121)
    void Process();   // 0x1012e230 (536 bytes, untranslated): a table of function pointers (0x102a4280) on the kind byte below 0x14
    void KillMe();    // 0x1012e728 (568 bytes, untranslated)
    void FinalDestroy();   // 0x1012e9f8 (496 bytes, untranslated; CParkAnalyser::FinalDestroy calls it)
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x1012e010 (444 bytes, Thing.cpp, untranslated; CParkAnalyser::Serialise calls it first)

    // Unit w122a (src/ThingCtors.cpp). Names are the unit's own: the cell x and y come from the arithmetic of the constructor, the list
    // links from CMapCell::InsertThing (0x10102d34).
    uint8_t mCellX;       // 0x04  the map cell's x: (cell index - 1) & 0x7f
    uint8_t mCellXSub;    // 0x05  0x80 at construction (the middle of the cell?)
    uint8_t mCellY;       // 0x06  the low byte of (cell index - 1) >> 7
    uint8_t mCellYSub;    // 0x07  0x80 at construction
    uint16_t mCellPrev;   // 0x08  the id of the thing before this one in the cell's list (CMapCell::InsertThing writes it); 0 at construction
    uint16_t mCellNext;   // 0x0a  the id of the thing after it (the cell's old head); 0 at construction
    CThing() = default;   // the other units' fakes and the tests' arrays (CThing gThings[64]) need a trivial default constructor
    // 0x1012de58 (236 bytes, src/ThingCtors.cpp): enters the thing in gThingArray (CWorld::AddThingToThingList on the global world) and in
    // the map cell list of cell `cell` (1 based map cell id: x = (cell - 1) & 0x7f, y = (cell - 1) >> 7; CMapCell::InsertThing).
    CThing(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, ThingModel model);
    // Unit w205 (src/World_save.cpp). 0x1013dce0 (4 bytes: a bare return): the constructor of a thing the loader fills in; it writes nothing.
    explicit CThing(const CSerialise& serialise);
    // Unit w207 (src/World_load.cpp). 0x1012dfa8 (16 bytes): stores the thing's id (the loader makes a thing's id from its table row).
    void ForceThingId(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);
    // Unit v5281. 0x1012e5b4 (324 bytes, stand-in): moves the thing to a position (two MapPos pairs).
    void Move(MapPos x, MapPos y);
};
static_assert(sizeof(CThing) == 0xc, "a thing's common part is 0xc bytes");

// An index type tagged with what it indexes; passed by value in one register (the
// PowerPC puts the 16-bit index in the high half of the word).
struct IDXTYPEUniqueNameMapId;
template<class Tag, class T> struct tag_IDX_TYPE {
    T mIndex;
    // Unit w160: Byteswap exchanges the two bytes in place (lhz; sthbrx). Only two instances are emitted in the binary:
    // ObjectId 0x100fa688 and ThingId 0x10079a0c (<unsigned short> index); defined in src/Serialise_basic.cpp.
    void Byteswap();
};

// The ride record's mUIType (unit w151, names mine): 0 ride, 1 shop, 2 sideshow, 3 feature; the queries also compare with 4 (AllObjectsResearchedAndPresent skips it).
enum UIObjType : int32_t { kUIObjRide = 0, kUIObjShop = 1, kUIObjSideshow = 2, kUIObjFeature = 3 };
// CObject::ShopType (unit w152): the ride record's kRideSlot_ShopType values; CObject aliases it (ThingReceivers.h).
enum CObjectShopType : int32_t {};
class CParkAnalyser {
public:

    // Unit w123a: direct dispatch callees, untranslated.
    void UpdateTurn();   // 0x100e4188
    void FinalDestroy();   // 0x100e6fd0 (36 bytes, src/ParkAnalyser_month.cpp, unit w161): CThing::FinalDestroy of its base
    CParkAnalyser();   // 0x100e2a4c (1,144 bytes, src/ParkAnalyser_ctor.cpp, unit w122b; 0x21c48 bytes on the Mac; CWorld::SetupNewLevel makes the analyser)
    void ReceiveMessage(const CMsg& message);   // 0x100e41bc (360 bytes, src/ParkAnalyser_month.cpp, unit w161; CThing::ReceiveMessage kind 13)

    // Unit w152 (src/ParkAnalyser_shops.cpp): the shop and sideshow superlatives. Each walks the used things, tests the objects (kind 3) older than
    // 30 and answers the best or worst value (percent of the income history's sum over the 1A4 history's sum, unsigned, kept as a signed word) or
    // customer satisfaction, 0 when no thing matched. The thing id of the winner is written through `id` (cleared first). All answer in r3.
    // The shops of one ShopType (the ride record's slot 86 equals `type`):
    int32_t GetShopWithBestValue(CObjectShopType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100eaa24 (476 bytes)
    int32_t GetShopWithWorstValue(CObjectShopType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100eac7c (476 bytes)
    int32_t GetShopWithBestSatisfaction(CObjectShopType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100eaed4 (348 bytes)
    int32_t GetShopWithWorstSatisfaction(CObjectShopType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100eb0b4 (348 bytes)
    // The shops of any food or drink type (the slot is 1, 5 or 6, three separate reads):
    int32_t GetShopWithBestValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100eb294 (520 bytes)
    int32_t GetShopWithWorstValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100eb504 (520 bytes)
    int32_t GetShopWithBestSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100eb778 (392 bytes)
    int32_t GetShopWithWorstSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100eb970 (392 bytes)
    // The sideshows (the ride record's mUIType is 2):
    int32_t GetSideshowWithBestValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100ebb68 (488 bytes)
    int32_t GetSideshowWithWorstValue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100ebdbc (488 bytes)
    int32_t GetSideshowWithBestSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;   // 0x100ec014 (416 bytes)
    // 0x100ec228 (488 bytes): the same code as GetSideshowWithWorstValue (an original copy-paste), it answers the value, not the satisfaction.
    int32_t GetSideshowWithWorstSatisfaction(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;
    // 0x100e5944 (420 bytes, src/ParkAnalyser_readers.cpp, unit w125): half the visitor count, 0 to 100. The count of the kind 1 things inside the park
    // is kept in mVal1F0B8 and recounted when the Val has no value yet or its clock says a second or more has passed; the answer itself is
    // GetVisitorsInPark(0) / 2 (asked up to three times), not the cached count.
    int32_t HowBusyIsPark() const;
    // 0x100e5414 (316 bytes, untranslated: a launcher stand-in answering 0): the visitors in the park; HowBusyIsPark passes 0.
    int32_t GetVisitorsInPark(uint32_t arg) const;
    // Unit w153 (src/ParkAnalyser_rides.cpp): the ride superlatives. Every walk is the used list of things; a ride is an object (kind 3) whose ride
    // record has mUIType 0. The ThingId& answers write 0 first and then the id of the best ride; ObjectId and ThingId arrive in the high halfword.
    // 0x100ec484 (376 bytes): the go-kart (track type 1) ride with the highest excitement level (unsigned byte compare); 0 when none.
    uint8_t GetMostExcitingGokart(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;
    // 0x100ec668 (384 bytes): the go-kart ride with most cross roads (the third out of BMP_CountTrackSections, signed compare); 0 when none.
    int32_t GetGokartWithMostCrossRoads(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;
    // 0x100ec858 (424 bytes): the track ride (CObject::IsTrackRide) of that object type (0x102a051c, a data halfword of 0, means any) with most track sections.
    int32_t GetLongestTrackRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const;
    // 0x100eca98 (384 bytes): the ride whose track type (record slot WhichTrackType) is `track` with most track sections.
    int32_t GetLongestTrackRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id, int32_t track) const;
    // 0x100ecc80 (376 bytes): the coaster of that object type (0x102a0522 means any) with most loops (unsigned); 0 when none.
    uint32_t GetCoasterWithMostLoops(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const;
    // 0x100ece94 (408 bytes): the coaster of that object type (0x102a0526 means any) with the highest second track statistic (a float, answered as unsigned).
    uint32_t GetHighestCoaster(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const;
    // 0x100ed0c0 (316 bytes; the original's spelling): the mean of GetNumPeopleInQueue over the objects with flag 1 (toilets), 0.0f when none.
    float GetAverageToiletQueueLenght() const;
    // 0x100ed240 (284 bytes, not const): the first object with flags 4 and 8 (choosable, has a queue) whose queue is not connected, and (unless `any`) that is not
    // the cell system's current object; a hidden halfword result, 0 when none.
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> FindRideWithUnconnectedQueue(bool any);
    // 0x100ed3a0 (400 bytes): the mean excitement level of the rides, cached in mVal1F0F4 for `seconds` seconds.
    int32_t GetAverageExcitementLevelOfAllRides(uint32_t seconds) const;
    // 0x100ed57c (344 bytes): the ride with the highest excitement level (unsigned byte), the id written once after the walk (no early write, no found flag).
    uint8_t GetScariestRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const;
    // 0x100e558c (344 bytes): the number of rides, cached in mVal1F0C4 for `seconds` seconds.
    int32_t GetRidesInPark(uint32_t seconds) const;
    // 0x100e7278 (240 bytes, src/ParkAnalyser_readers.cpp, unit w125): counts the kind 1 things (the dead flag is not tested) for which CPeep::IsInsidePark
    // answers nonzero.
    int32_t NumberOfVisitorsInPark() const;
    // Unit w154 (src/ParkAnalyser_visitors.cpp): the visitor counts and opinions. CountNeighboursOfType (below) is translated in src/ParkAnalyser_readers.cpp
    // (unit w125) and GetVisitorsInPark (above) is translated here. The walks of the used things count the kind 1 things for which CPeep::IsInsidePark
    // answers nonzero; a float level of the peep is read as the low byte of its fctiwz.
    void OfferOpinionOn(int32_t opinion, uint8_t value);       // 0x100e4360 (60 bytes): appends `value` to the history of that opinion
    void IveMisbehaved(int32_t caught);                        // 0x100e43e4 (52 bytes): counts a misbehaving kid (and, when `caught` is 0, one that got away with it)
    void PersonLeftPark(int32_t turns);                        // 0x100e4600 (112 bytes): appends the whole days of the stay (that many game turns) to mHistory21600
    float GetHappinessLevel(uint32_t unused) const;            // 0x100e4cd8 (424 bytes): the mean happiness of the visitors in the park, 0.0f when the gates are shut or none; the argument is not read
    int32_t NumberOfVisitorsOutsidePark() const;               // 0x100e73a8 (308 bytes): the visitors in state 3 on the two ticket booth cells, kept in mVal1F0D0
    int32_t NumberOfVisitorsNeedingToilet(uint8_t level) const;   // 0x100e7520 (280 bytes): visitors in the park whose toilet need byte is above `level`
    int32_t NumberOfHungryVisitors(uint8_t level) const;       // 0x100e7680 (280 bytes): the same for the hunger byte
    int32_t NumberOfThirstyVisitors(uint8_t level) const;      // 0x100e77d8 (280 bytes): the same for the thirst byte
    int32_t MisbehavingVisitorsLastMonth(bool gotAway) const;  // 0x100e7930 (32 bytes): mMisbehavingKidsThatGotAwayWithIt or mMisbehavingKids
    int32_t NumberOfVisitorsSinceBeginning() const;            // 0x100e7994 (12 bytes): mLifetimeVisitors
    int32_t NumberOfVisitorsInLastMonths(int32_t back) const;  // 0x100e79e8 (104 bytes): mLifetimeVisitors minus the history's value `back` places back, 0 when it has none
    int32_t NumberOfVisitorsWithBalloons() const;              // 0x100e7a94 (256 bytes): visitors in the park holding a balloon
    int32_t NumberOfVisitorsWithCostumes() const;              // 0x100e7bd8 (256 bytes): visitors in the park wearing a costume (the word at +0x24 is 2)
    int32_t PercentageOfVisitorsWithHappinessOver(int32_t level) const;    // 0x100e7d1c (304 bytes): percent of the visitors in the park with happiness byte above `level` (signed)
    int32_t PercentageOfVisitorsWithHappinessUnder(int32_t level) const;   // 0x100e7e98 (304 bytes): the same below `level`
    uint8_t GetAverageOpinionOn(int32_t opinion) const;        // 0x100e8018 (68 bytes): the mean of the last 50 values of that opinion
    bool HaveCompleteSetOfOpinions(int32_t opinion) const;     // 0x100e80a8 (76 bytes): the opinion's history holds more than 49 values
    // 0x100ef00c (372 bytes): the mean happiness byte of the kind 1 things (no IsInsidePark test) in the square of cells within `radius` of `cell`;
    // the low bounds are NOT clamped at 0 (a negative start reads the cells before the row), 0 when none.
    int32_t GetAverageHappinessAroundPoint(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, uint32_t radius) const;
    // 0x100e6a6c (424 bytes, src/ParkAnalyser_readers.cpp, unit w125; `this` is not used): the things of model `model` in the square of map cells
    // around cell `id` and within `a` cells of it (clamped to the 128 x 128 map): their count, or 1 at the first one when `b` is not 0.
    int32_t CountNeighboursOfType(CThing::ThingModel model, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id,
                                  int32_t a, int32_t b);
    // Unit w156 (src/ParkAnalyser_cells.cpp): the cell and map queries. A map cell id is 1 based (x = (id - 1) & 0x7f, y = (id - 1) >> 7); the full-map
    // loops run x from 0x7f down to 0, y from 0x7f down to 0, and gWorld's map is the 0x4000 cells at gWorld + 0x2d8.
    // AreaType (the mangling says CParkAnalyser::AreaType; the names are mine): the cells PercentageOfAreaPatrolledBy counts.
    enum AreaType : int32_t { kAreaLand = 0, kAreaBuilt = 1, kAreaEntrance = 2, kAreaPath = 3 };
    // 0x100e4c38 (64 bytes): the analyser cell that covers map cell `id` (a doubt: the return type is not in the mangling).
    CParkAnalyserCell* GetAnalyserCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id) const;
    // 0x100e571c, 0x100e57d4 and 0x100e588c (124 bytes each): the sums over the 0x400 analyser cells (from the last down) of the cell's counts of
    // walkable, littered and developed map cells, each recounted when older than `maxAge` seconds.
    int32_t GetNumWalkableCells(uint32_t maxAge) const;
    int32_t GetNumLitteredCells(uint32_t maxAge) const;
    int32_t GetNumDevelopedCells(uint32_t maxAge) const;
    // 0x100e6c90 (44 bytes): CountNeighboursOfType(model, id, a, 1), that is whether a thing of that model is within `a` cells.
    int32_t IsNearbyThingOfType(CThing::ThingModel model, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id, int32_t a);
    // 0x100e6d38 (516 bytes; a hidden halfword result): the cell within `radius` of `id` (gWorldMap) with litter that `who` (or the halfword 0x102a04c4,
    // 0) is to collect and that is nearest to it (Manhattan distance, the first on a tie), 0 when none; the cells found are listed in mNearbyCells,
    // 0x24 entries, a larger radius writes past it.
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> GetNearbyLitteredCell(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> who, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> id, int32_t radius);
    // 0x100ed738 (336 bytes): percent of the walkable or littered cells of GetWorld()'s map that are littered, 0 when there are none.
    int32_t GetPercentageLitteredCells() const;
    // 0x100eee30 (108 bytes): clears the patrol mark (CMapCell::mF3C) of every cell of gWorld's map.
    void ClearMapData();
    // 0x100ee694 (576 bytes): ClearMapData, then marks (mF3C = 1) the rectangle of cells of the patrol area of every staff thing (kinds 4 to 8)
    // whose kind equals `model` and for which CStaff::HasPatrolArea answers nonzero.
    void MarkAreaPatrolledBy(CThing::ThingModel model);
    // 0x100ee924 (836 bytes): MarkAreaPatrolledBy(model), then the percent of the cells of `area` that are marked; 0 for any other area.
    int32_t PercentageOfAreaPatrolledBy(CThing::ThingModel model, AreaType area);
    // 0x100eecd8 (272 bytes): percent of the land cells (not water, freeform or lake) whose region effect cell has a positive effect [3] (cameras).
    int32_t PercentageOfParkCoveredByCameras();
    // 0x100eeed0 (248 bytes): the cells of gWorld's map that are an entrance, exit, ride, track or coaster pylon.
    int32_t NumberOfCellsCoveredByObjects();
    int32_t Init();   // 0x100e4a44 (456 bytes, untranslated): subscribes and sets up the cells
    void BuiltSomething();   // 0x100e46a4 (120 bytes, src/Things2_small.cpp, unit w130): stamps mLastObjectBuilt with the park's funny time; CObject::CObject calls it last
    // 0x100e8144 (404 bytes, src/Things2_small.cpp, unit w130): how many things of kinds 4 to 8 (the staff) of model `model` are alive.
    int32_t NumberOfStaffType(CThing::ThingModel model) const;
    // Unit w155 (src/ParkAnalyser_staff.cpp): the staff queries. Staff are the things of kinds 4 to 8; the walks are the used list's.
    void HiredSomeone();   // 0x100e4750 (120 bytes): stamps mLastStaffHired with the funny time of the global world gWorld
    // 0x100e4ebc (532 bytes): the mean happiness byte of the staff (0.0f when none), the sum carried through single precision adds; the argument is not used.
    float GetStaffHappinessLevel(uint32_t unused) const;
    int32_t GetDaysSinceLastObjectBuilt() const;   // 0x100e7098 (172 bytes): whole days from mLastObjectBuilt to the funny time of GetWorld()
    int32_t GetDaysSinceLastStaffHired() const;    // 0x100e7188 (172 bytes): the same from mLastStaffHired
    int32_t NumberOfStaff() const;                 // 0x100e8324 (380 bytes): the things of kinds 4 to 8 (dead ones too)
    int32_t NumberOfStaffTooTiredToWork() const;   // 0x100e84d4 (396 bytes): the staff for whom CStaff::TooTiredToWork answers nonzero
    // 0x100e86a4 (416 bytes): the staff of model `model` (any when 0) for whom CStaff::HasPatrolArea answers nonzero
    int32_t NumberOfStaffTypeWithPatrolArea(CThing::ThingModel model) const;
    // 0x100e88a0 (264 bytes): the things of kind == model (no staff test): (sum of the grades * 100) / (count * 5), 0 when none
    int32_t AverageQualityOfStaffType(CThing::ThingModel model) const;
    int32_t AverageHappinessOfStaffType(CThing::ThingModel model) const;   // 0x100e89fc (272 bytes): mean happiness byte of the things of kind == model (no staff test)
    int32_t AverageHappinessOfStaff() const;       // 0x100e8b64 (424 bytes): mean happiness byte of the staff
    int32_t CurrentNumberOfStaffTypeForHire(StaffType type) const;   // 0x100e8d4c (60 bytes): the staff database's count of that type
    int32_t CurrentResearcherAbility() const;      // 0x100e8dd8 (192 bytes): the balance's research ability of each researcher's grade, summed
    int32_t MaxResearcherAbility() const;          // 0x100e8ed8 (20 bytes): balance slots 172 times 273
    // Unit w129 (src/ParkAnalyser_gates.cpp). 0x100e7028 (64 bytes): the park's gates are open: CWorld::IsParkClosed() of the world CWorldSavvy answers is 0.
    bool GatesOpen() const;
    // Unit w159 (src/ParkAnalyser_rating.cpp)
    // 0x100ed8cc (3,468 bytes): the park rating, the sum of 11 capped terms (visitors, four ride classes, five staff kinds); each term is walked inline and
    // the real counting function is called too when the walk's count is under the cap.
    int32_t GetOverallParkRating() const;
    // 0x100e5110 (720 bytes): the park value cached in mVal1F0E8 for `seconds` seconds: points per map cell kind plus the upgrade costs of every object scaled by its mF40.
    int32_t GetParkValue(uint32_t seconds) const;
    // 0x100e5b1c (616 bytes, not const on the Mac): the sum over the rides and shops of the object list of (the attraction value plus the new bonus) divided by their count.
    int32_t GetTotalParkAttractionPoints();

    // Unit w151 (src/ParkAnalyser_objects.cpp): the queries over the park's objects and the ride database. UIObjType is the ride record's mUIType
    // (names mine). The ObjectId and ThingId arguments are passed by the Mac in the high halfword of a register.
    int32_t NumberOfToiletsInPark() const;                // 0x100e8f28 (268 bytes): the objects with flag 1, kept in mVal1F0DC; answers what Val::Get answers after the Set
    int32_t NumberOfClosedToiletsInPark() const;          // 0x100e9070 (252 bytes): the toilets that are closed
    int32_t NumberOfStaffRoomsInPark() const;             // 0x100e91b0 (236 bytes): the objects with flag 2
    int32_t NumberOfClosedStaffRoomsInPark() const;       // 0x100e92dc (252 bytes): the staff rooms that are closed
    int32_t AvgFilthLevelOfToilets() const;               // 0x100e9420 (288 bytes): the mean of 100 - (byte of the cleanliness float) over the toilets, 0 when none
    // 0x100e9580 (348 bytes): the filthiest toilet's id (a hidden halfword result); `filth` is set to 0 first and then to its filth level
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> FilthiestToilet(int32_t& filth) const;
    int32_t NumberOfBrokenDownRidesInPark() const;        // 0x100e9714 (236 bytes): the ride type objects of the object list that are broken down
    int32_t NumberOfWornRidesInPark(int32_t level) const; // 0x100e9844 (276 bytes): the ride type objects of the object list worn below `level` or broken down
    int32_t NumberOfObjectTypeInPark(UIObjType type, int32_t level) const;   // 0x100ea5d8 (292 bytes)
    int32_t NumberOfClosedObjectTypeInPark(UIObjType type) const;            // 0x100ea748 (288 bytes)
    // 0x100ea3b0 (264 bytes): the highest upgrade level (byte at +0x4c) among the objects of that type, -1 when there is none
    int32_t GetHighestUpgradeLevelForObject(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const;
    int32_t TypesOfRidesAvailable(bool includeGoldenTickets) const;       // 0x100e9998 (192 bytes): researched ride types (golden ticket ones only when asked)
    int32_t TypesOfUpgradesAvailable(bool unused) const;                  // 0x100e9a94 (184 bytes): ride types with upgrades available (the byte argument is not read)
    int32_t TypesOfShopsAvailable(bool unused) const;                     // 0x100e9b8c (184 bytes): researched shop types
    int32_t TypesOfSideshowsAvailable(bool unused) const;                 // 0x100e9c80 (184 bytes)
    int32_t TypesOfFeaturesAvailable(bool unused) const;                  // 0x100e9d78 (184 bytes)
    int32_t TypesOfRidesLeftToResearch() const;           // 0x100e9e70 (56 bytes): the research lab's GetNumberLeftToResearch(0)
    int32_t TypesOfShopsLeftToResearch() const;           // 0x100e9eec (56 bytes): category 1
    int32_t TypesOfSideshowsLeftToResearch() const;       // 0x100e9f68 (56 bytes): category 2
    int32_t TypesOfFeaturesLeftToResearch() const;        // 0x100e9fe8 (56 bytes): category 3
    bool AllObjectsResearchedAndPresent() const;          // 0x100ea064 (232 bytes): nothing left to research (category 5) and every non-feature type is in the park or a golden ticket one
    int32_t PercentageOfTotalObjectsInPark() const;       // 0x100ea194 (188 bytes): the golden ticket object types with one in the park, as a percentage
    int32_t TypesOfParticularObjectTypeInPark(UIObjType type, bool includeGoldenTickets) const;   // 0x100ea298 (196 bytes)
    int32_t NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const;      // 0x100ea52c (68 bytes): the object control's count
    // 0x100ea8b8 (100 bytes): the history's value `back` places back (a hidden halfword result, 0 when the history has none)
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> LastOfObjectTypeResearched(UIObjType type, int32_t back) const;
    void SetLastOfObjectTypeResearched(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id);   // 0x100ea968 (64 bytes)
    // Unit w161 (src/ParkAnalyser_month.cpp).
    void PersonPaidAdmission(int32_t price);   // 0x100e4470 (216 bytes): counts the visitor, keeps the largest price, posts a CMsgAddToChallenge (type 0xb, value 1)
    void UpdateEndOfMonth();   // 0x100e47fc (528 bytes): the month-end history entries (CMsgEndOfMonth), then the misbehaving counters are cleared
    void GetThreeMostDominantThoughts(uint8_t& first, uint8_t& second, uint8_t& third) const;   // 0x100e5dc8 (576 bytes): the three most common thoughts of the visitors in the park, 0x17 for none
    // Unit w162 (src/ParkAnalyser_serialise.cpp).
    int32_t Serialise(void* file, int32_t save, int32_t version, uint32_t unused);   // 0x100e3004 (4,432 bytes): CThing::Serialise of the base, then (0 <= version < 2) every saved field loaded or saved in order; the first nonzero answer ends it with 1

    CThing mBase;                                   // 0x00  (w122a's CThing, 0xc bytes)
    int32_t mMapSquaresPerCell;                     // 0x0c  "mMapSquaresPerCell": 4
    int32_t mGridSizeX;                             // 0x10  "mGridSizeX": 0x20
    int32_t mGridSizeY;                             // 0x14  "mGridSizeY": 0x20
    int32_t mMapSizeX;                              // 0x18  "mMapSizeX": 0x80
    int32_t mMapSizeY;                              // 0x1c  "mMapSizeY": 0x80
    CParkAnalyserCell mCells[0x400];                // 0x20  (GetAnalyserCell 0x100e4c38 indexes it)
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> mNearbyCells[0x24];   // 0x1f020  scratch list of cell ids filled by GetNearbyLitteredCell (name mine); never initialised
    CHistory<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>, 3> mLastOfObjectType[4];   // 0x1f068  (SetLastOfObjectTypeResearched indexes it by the UIObjType; name mine)
    CParkAnalyserVal<int> mVal1F0B8;                // 0x1f0b8  six cached park values (names mine), each with its own refresh clock
    CParkAnalyserVal<int> mVal1F0C4;                // 0x1f0c4
    CParkAnalyserVal<int> mVal1F0D0;                // 0x1f0d0
    CParkAnalyserVal<int> mVal1F0DC;                // 0x1f0dc
    CParkAnalyserVal<int> mVal1F0E8;                // 0x1f0e8
    CParkAnalyserVal<int> mVal1F0F4;                // 0x1f0f4
    CHistory<int, 144> mHistory1F100[13];           // 0x1f100  stride 0x250 (names mine): the mTemp word of [2] (0x1f5a0) is the total CBankAccount withdrawals add to, of [5] (0x1fc90) the total its deposits add to, of [6] (0x1fee0) admissions
    CHistory<float, 144> mHistory20F10;             // 0x20f10
    CHistory<int, 144> mHistory21160[2];            // 0x21160
    CHistory<int, 50> mHistory21600;                // 0x21600
    CHistory<int, 5> mHistory216D8;                 // 0x216d8
    CHistory<unsigned char, 50> mOpinionHistory[0x14];   // 0x216fc  stride 0x40 (OfferOpinionOn 0x100e4360 calls SetAtEnd on element `opinion`; name mine)
    int32_t mMisbehavingKids;                       // 0x21bfc  "mMisbehavingKids"
    int32_t mMisbehavingKidsThatGotAwayWithIt;      // 0x21c00  "mMisbehavingKidsThatGotAwayWithIt"
    int32_t mMisbehavingKidsEver;                   // 0x21c04  "mMisbehavingKidsEver"
    int32_t mLifetimeVisitors;                      // 0x21c08  "mLifetimeVisitors"
    uint32_t mLastStrike[2];                        // 0x21c0c  "mLastStrike": a TbTimeStamp held as two words (as CParkTime.h does)
    uint32_t mLastPuke[2];                          // 0x21c14  "mLastPuke"
    uint32_t mLastPrank[2];                         // 0x21c1c  "mLastPrank"
    uint32_t mParkLastOpened[2];                    // 0x21c24  "mParkLastOpened"
    uint32_t mLastObjectBuilt[2];                   // 0x21c2c  "mLastObjectBuilt"
    uint32_t mLastStaffHired[2];                    // 0x21c34  "mLastStaffHired"
    int32_t mMostPaidForTicket;                     // 0x21c3c  "mMostPaidForTicket"
    int32_t mLargestBusLoad;                        // 0x21c40  "mLargestBusLoad"
    int32_t mLongestStay;                           // 0x21c44  "mLongestStay"
};

void SCR_CloseScriptSystem();      // 0x100d8e4c (148 bytes, src/ScriptTick.cpp, unit w132)
// The script variable setters (the script object is a handle into the script system's table; nothing happens when the
// handle is not in it or the system is off). SCR_SetVariable and SCR_SetFloatVariable take an index of 0 to 0x12.
// 0x100d9478 (116 bytes, src/analyser_kill.cpp, unit w114): when the script system is on and 0 < handle < gScriptTableSize, clears the
// table entry and frees the script block it held (the entry is cleared first). The table's words are 32 bits (see gScriptTable), so a
// host pointer cannot be held in them yet.
void SCR_KillScript(int32_t handle);
void SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode);   // 0x100d932c (92 bytes, src/ScriptInterp.cpp, unit w163)
void SCR_SetVariable(int32_t handle, uint32_t index, int32_t value);                  // 0x100d9514 (96 bytes, src/ScriptInterp.cpp, unit w163)
void SCR_SetFloatVariable(int32_t handle, uint32_t index, float value);               // 0x100d95a0 (96 bytes, src/ScriptInterp.cpp, unit w163)
void SCR_ScriptTick();             // 0x100d907c (224 bytes, src/ScriptTick.cpp, unit w132)
void SCR_BuildScriptSpriteList();  // 0x100d9184 (152 bytes, src/ScriptTick.cpp, unit w132)
int32_t DoScript(struct tagSCRIPT* script);  // 0x100d88b4 (276 bytes, src/ScriptInterp.cpp, unit w163: runs one script and answers a count that SCR_ScriptTick sums)
// ---- Unit w132 (src/ScriptTick.cpp), laid out in full by unit w163 (src/ScriptInterp.cpp): the script block the table words point at
// (0x118 bytes; LbMem_Alloc(0x118) in SCR_SpawnScript, SCR_NewScriptBlock here). Code addresses (mCodeBase, the jump operands) are Mac
// addresses in 32-bit words, see SCR_CodeWord below. Floats live in the variable words: read and write them with std::memcpy.
struct tagSCRIPT {
    uint32_t mText;       // +0x00  PK_SETTEXT's operand
    int32_t mSlot;        // +0x04  the handle (its slot in gScriptTable, set by AssignSlot)
    int32_t mPos;         // +0x08  the index of the next code word, counted from mCodeBase
    int32_t mStartPos;    // +0x0c  mPos at the start
    int32_t mLastPos;     // +0x10  the instruction being run (DoScript)
    uint32_t mCodeBase;   // +0x14  a Mac code address (kScriptCodeAddress)
    int32_t mState;       // +0x18  1 running, 2 waiting (PK_WAIT), 3 never run, 4 finished (SCR_ScriptTick kills it)
    int32_t mStackTop;    // +0x1c  0x14 when empty; SCR_Push counts it down
    int32_t mStack[0x14]; // +0x20
    int32_t mIfDepth;     // +0x70  the IF nesting (PK_IF counts it up, PK_ENDIF down; zeroed at start)
    int32_t mGosubDepth;  // +0x74  PK_GOSUB / PK_RETURN nesting
    int32_t mLoopDepth;   // +0x78  the DO / REPEAT nesting (zeroed at start)
    int32_t mNextTime;    // +0x7c  the clock value after which it runs next
    int32_t mPeriod;      // +0x80  added to the clock after a run (0x3e at start)
    int32_t mVars[0x13];  // +0x84  variables; 1 to 3 the position x, y, z (floats), 4 to 6 floats, 7 = 0xff, 8 and 9 floats, 15 the mode
    uint8_t mSprite[0x44]; // +0xd0  the embedded sprite record (opaque for now; +0xf8 inside it is zeroed at start)
    int32_t mHasSprite;   // +0x114  non-zero when mSprite is in use (PK_END clears it)
};
static_assert(offsetof(tagSCRIPT, mSlot) == 0x04 && offsetof(tagSCRIPT, mPos) == 0x08 && offsetof(tagSCRIPT, mStartPos) == 0x0c &&
              offsetof(tagSCRIPT, mLastPos) == 0x10 && offsetof(tagSCRIPT, mCodeBase) == 0x14 && offsetof(tagSCRIPT, mState) == 0x18 &&
              offsetof(tagSCRIPT, mStackTop) == 0x1c && offsetof(tagSCRIPT, mStack) == 0x20 && offsetof(tagSCRIPT, mIfDepth) == 0x70 &&
              offsetof(tagSCRIPT, mGosubDepth) == 0x74 && offsetof(tagSCRIPT, mLoopDepth) == 0x78 && offsetof(tagSCRIPT, mNextTime) == 0x7c &&
              offsetof(tagSCRIPT, mPeriod) == 0x80 && offsetof(tagSCRIPT, mVars) == 0x84 && offsetof(tagSCRIPT, mSprite) == 0xd0 &&
              offsetof(tagSCRIPT, mHasSprite) == 0x114, "tagSCRIPT keeps the Mac's offsets");
static_assert(sizeof(tagSCRIPT) == 0x118, "tagSCRIPT is 0x118 bytes");
extern int32_t gScriptTickTotal;  // 0x102f7c08  TOC 0x10261110  the sum of the DoScript results of the last tick
extern int32_t gScriptTickTime;   // 0x102f7c0c  TOC 0x10261114  the clock value read by the last tick
// ---- Unit w163 (src/ScriptInterp.cpp, src/ScriptBlocks.cpp): the interpreter's core
// The script code is ONE array of 32-bit words in the Mac's data segment; an instruction is the address of a command descriptor followed by
// its operands. Code addresses stay Mac addresses (uint32_t) on the host; gScriptCodeWords is the host copy of the array (see SCR_CodeWord).
constexpr uint32_t kScriptCodeAddress = 0x1029f20c;      // the code array's Mac address (TOC 0x10261140)
constexpr uint32_t kScriptCommandAddress = 0x10266958;   // the first command descriptor (TOC 0x1026111c); 8 bytes each, 42 of them
extern tagSCRIPT* gScriptCurrent;    // 0x102f7c24  the script DoScript runs; every PK_ function reads it
extern uint32_t gScriptCodeBase;     // 0x102f7c20  the current script's mCodeBase
extern int32_t* gScriptVars;         // 0x102f7c1c  the current script's mVars
extern const uint32_t gScriptStartAddresses[4];   // 0x1029ffdc  the start addresses of the custom data scripts (ESP_SpawnScript)
// The code word at a Mac code address: the host array's word when the address lies in it, else 0 (the Mac reads whatever is there).
uint32_t SCR_CodeWord(uint32_t address);
// The Mac code address of a host pointer into gScriptCodeWords, 0 for a pointer outside it.
uint32_t SCR_CodeAddress(const void* code);
// Host only (src/ScriptBlocks.cpp): the Mac's LbMem_Alloc(0x118) and LbMem_Free of a script block, which must come from the arena (the
// table's words are Ptr32s). Unit libraries fake the pair.
tagSCRIPT* SCR_NewScriptBlock();
void SCR_FreeScriptBlock(tagSCRIPT* script);
enum ESP_Sprites : int32_t;
enum ACTS : int32_t;
int32_t AssignSlot(tagSCRIPT* script);   // 0x100d8630 (228 bytes): the first free handle (the table grows by 0x32 words when full)
void InitScript(tagSCRIPT* script, void* code, int32_t value, float x, float y, float z);   // 0x100d8768 (280 bytes)
void SCR_Error(const char* message);     // 0x100d8740 (4 bytes, empty)
void SCR_Push(int32_t value);            // 0x100d8cdc (56 bytes)
int32_t SCR_Pop();                       // 0x100d8d34 (64 bytes)
int32_t SCR_SpawnScript(void* code, int32_t value, float x, float y, float z);   // 0x100d924c (180 bytes)
void SCR_GetScriptPosition(int32_t handle, float* x, float* y, float* z, int32_t* mode);   // 0x100d93b8 (140 bytes)
void SCR_SetServiceInterval(int32_t handle, int32_t period);   // 0x100d9630 (80 bytes)
int32_t SCR_WhichScriptHasThisSprite(void* sprite);   // 0x100da7e0 (112 bytes)
void ESP_InitScript(tagSCRIPT* script, void* code, ESP_Sprites set, int32_t index, ACTS acts, float x, float y, float z, int32_t a, int32_t b);   // 0x100da008 (312 bytes)
// The 42 command functions, in descriptor order (index (descriptor - kScriptCommandAddress) / 8); each is the code address in its descriptor.
void PK_GOTO();              // 0x100da980  descriptor 0 (src/ScriptInterp.cpp, unit w163)
void PK_IF();                // 0x100da9d8  descriptor 1 (src/ScriptFlow.cpp, unit w164)
void PK_ENDIF();             // 0x100dadc8  descriptor 2 (src/ScriptFlow.cpp, unit w164)
void PK_ELSE();              // 0x100dae38  descriptor 3 (src/ScriptFlow.cpp, unit w164)
void PK_GOSUB();             // 0x100daea0  descriptor 4 (src/ScriptInterp.cpp, unit w163)
void PK_RETURN();            // 0x100daf6c  descriptor 5 (src/ScriptInterp.cpp, unit w163)
void PK_DO();                // 0x100db588  descriptor 6 (src/ScriptFlow.cpp, unit w164)
void PK_PUSHVAR();           // 0x100db48c  descriptor 7 (src/ScriptInterp.cpp, unit w163)
void PK_POPVAR();            // 0x100db508  descriptor 8 (src/ScriptInterp.cpp, unit w163)
void PK_LOOP();              // 0x100db620  descriptor 9 (src/ScriptFlow.cpp, unit w164)
void PK_REPEAT();            // 0x100daff8  descriptor 10 (src/ScriptFlow.cpp, unit w164)
void PK_UNTIL();             // 0x100db060  descriptor 11 (src/ScriptFlow.cpp, unit w164)
void PK_END();               // 0x100db6f0  descriptor 12 (src/ScriptInterp.cpp, unit w163)
void PK_WAIT();              // 0x100db714  descriptor 13 (src/ScriptInterp.cpp, unit w163)
void PK_NOP();               // 0x100da95c  descriptor 14 (src/ScriptInterp.cpp, unit w163)
void PK_SETTEXT();           // 0x100db748  descriptor 15 (src/ScriptInterp.cpp, unit w163)
void PK_SETVAR();            // 0x100db79c  descriptor 16 (src/ScriptInterp.cpp, unit w163)
void PK_ADD();               // 0x100db864  descriptor 17 (src/ScriptFlow.cpp, unit w164)
void PK_SUB();               // 0x100db93c  descriptor 18 (src/ScriptFlow.cpp, unit w164)
void PK_AND();               // 0x100dbdc0  descriptor 19 (src/ScriptFlow.cpp, unit w164)
void PK_MUL();               // 0x100dba14  descriptor 20 (src/ScriptFlow.cpp, unit w164)
void PK_MULVAR();            // 0x100dbaec  descriptor 21 (src/ScriptFlow.cpp, unit w164)
void PK_DIV();               // 0x100dbba0  descriptor 22 (src/ScriptFlow.cpp, unit w164)
void PK_DIVVAR();            // 0x100dbc80  descriptor 23 (src/ScriptFlow.cpp, unit w164)
void PK_MOD();               // 0x100dbd38  descriptor 24 (src/ScriptFlow.cpp, unit w164)
void PK_MODVAR();            // 0x100dbe5c  descriptor 25 (src/ScriptFlow.cpp, unit w164)
void PK_ADDVAR();            // 0x100dbeec  descriptor 26 (src/ScriptFlow.cpp, unit w164)
void PK_COPYVAR();           // 0x100dbf9c  descriptor 27 (src/ScriptFlow.cpp, unit w164)
void PK_RANDOM();            // 0x100dc01c  descriptor 28 (src/ScriptFlow.cpp, unit w164)
void PK_SPAWN();             // 0x100dc0e0  descriptor 29 (src/ScriptSprites.cpp, unit w165)
void PK_SET_OBJ_ID();        // 0x100dc160  descriptor 30 (src/ScriptSprites.cpp, unit w165)
void PK_SET_OBJ_ACT();       // 0x100dc1b8  descriptor 31 (src/ScriptSprites.cpp, unit w165)
void PK_PutSpriteVAR();      // 0x100dc214  descriptor 32 (src/ScriptSprites.cpp, unit w165)
void PK_PutSpriteIM();       // 0x100dc2d0  descriptor 33 (src/ScriptSprites.cpp, unit w165)
void PK_Put3DSpriteIM();     // 0x100dc384  descriptor 34 (src/ScriptSprites.cpp, unit w165)
void PK_RemoveSprite();      // 0x100dc418  descriptor 35 (src/ScriptSprites.cpp, unit w165)
void PK_ResetScale();        // 0x100dc45c  descriptor 36 (src/ScriptSprites.cpp, unit w165)
void PK_ReduceScale();       // 0x100dc494  descriptor 37 (src/ScriptSprites.cpp, unit w165)
void PK_IncreaseScale();     // 0x100dc4d8  descriptor 38 (src/ScriptSprites.cpp, unit w165)
void PK_ResetRotation();     // 0x100dc51c  descriptor 39 (src/ScriptSprites.cpp, unit w165)
void PK_ReduceRotation();    // 0x100dc558  descriptor 40 (src/ScriptSprites.cpp, unit w165)
void PK_IncreaseRotation();  // 0x100dc59c  descriptor 41 (src/ScriptSprites.cpp, unit w165)
// ---- Unit w164 (src/ScriptFlow.cpp): the interpreter's flow and arithmetic
// The code end the two finders stop at: a Mac code address (TOC 0x10261128), compared as a signed word.
constexpr uint32_t kScriptCodeEnd = 0x1029ffd8;
void SCR_FindNextEndif();        // 0x100d89f4 (432 bytes): moves mPos to the next ENDIF at this nesting
void SCR_FindNextElseOrEndif();  // 0x100d8bcc (224 bytes): moves mPos to the next ELSE (and over it) or the next ENDIF
void DC_TickDynamicCellMeshes();   // 0x100dfd88
void STATIC_WhiteOut();            // 0x100e28b4

// ---- the script system and the dynamic cell meshes (src/AnalyserSystems.cpp, unit w111). Names invented; Mac addresses given.
class CDynamicMesh;
struct sDTextureSet;
// 0x100d8d94 (140 bytes): once (the first call sets gScriptInitialised): the word at 0x102f7c2c = 0x32, a table of 100 words
// (400 bytes from LbMem_Alloc, zeroed), then SCR_InitCheckTable.
void SCR_InitScriptSystem();
// 0x100d8f0c (212 bytes, src/ScriptCheckTable.cpp, unit w112c): one byte for each code word of the scripts, 1 when the word lies
// between the smallest and the largest command descriptor (a command), else 0; frees the old table first.
void SCR_InitCheckTable();
void SCR_CloseCheckTable();                 // 0x100d900c (68 bytes): frees the table and clears the pointer
// The script data the table is built from (src/ScriptCheckTable.cpp). The Mac words are addresses in its data segment (the code
// array of the compiled scripts and the zero ended list of command descriptors); the host holds the image's words (unit v5L39).
extern uint8_t* gScriptCheckTable;          // 0x102f7c10  the table (NULL: none)
extern const uint32_t* gScriptCodeWords;    // 0x1029f20c  the first code word (TOC slot 0x10261140)
extern const uint32_t* gScriptCodeWordsEnd; // 0x1029ffd8  one past the last (TOC slot 0x10261128, 883 words on the Mac)
extern const uint32_t* gScriptCommandList;  // 0x1029ffec  the command descriptors, ended by a 0 (TOC slot 0x10261118, 42 on the Mac)
extern uint32_t gScriptCommandFirst;        // 0x10266958  the first descriptor, the start of the range (TOC slot 0x1026111c)
extern uint32_t gScriptInitialised;         // 0x102f7c28
extern uint32_t gScriptF7C2C;               // 0x102f7c2c  0x32 once the system is on
extern int32_t gScriptTableSize;            // 0x102f7c14  100
extern void* gScriptTable;                  // 0x102f7c18  gScriptTableSize words
// ---- Unit w165 (src/ScriptSprites.cpp, src/ScriptSave.cpp): the script sprites, the save and load
// Handles outside 1 to gScriptTableSize - 1 find no script; the ESP_ functions with the system off (gScriptInitialised 0) do nothing.
// A code argument of 0 to 3 is a custom data block number (see ESP_SpawnScript), any other value a pointer into the code array.
void ESP_ForceGoto(int32_t handle, void* code);                  // 0x100da318 (300 bytes): restarts a script at a code address or custom block
int32_t ESP_RURunningThis(int32_t handle, void* code);           // 0x100da46c (248 bytes): 1 when the script started at that address
int32_t ESP_Spawn2DCopyOfScriptAt(int32_t handle, float x, float y, float z, float scale);   // 0x100da590 (276 bytes)
void ESP_Update2DScript(int32_t handle, int32_t source);         // 0x100da6d8 (220 bytes): copies the sprite fields of `source`
void ESP_FixSpriteIDsFromDetail();                               // 0x100da884 (164 bytes): sprite number modulo the set's sprite count
void SCR_SaveAllScripts(void* file);     // 0x100d96b0 (580 bytes, src/ScriptSave.cpp): tag, record size, table size, table, every block
int32_t SCR_LoadAllScripts(void* file);  // 0x100d9ccc (784 bytes, src/ScriptSave.cpp): 0 when loaded, 1 when not (the table is then fresh or short)
void DrawHLSprite(tagSCRIPT* script);    // 0x1007b114 (untranslated): draws the script's sprite record
int32_t ESP_GetNumberOfSprites(ESP_Sprites set);   // 0x1007c8b0 (untranslated): the number of sprites in a set
// The file swaps of the save and load: the main app's own instances of the SamsUtilities templates (swap on the Mac, no-ops on a
// little-endian host, where files already hold the host's order). SSpriteGameData::Byteswap works on the 0x44 byte sprite record.
namespace ScriptSaveSwap {
void USwapInplace32Int(int32_t& word);                  // 0x100d9920 USwapInplace32<int>
void SpriteRecordByteswap(uint8_t* record);             // 0x100d9964 SSpriteGameData::Byteswap
void USwapBlock32SLONGfloat(int32_t* words, int32_t count);   // 0x100d9b00 USwapBlock32<SLONGfloat>
void USwapBlock32Long(int32_t* words, int32_t count);   // 0x100d9b74 USwapBlock32<long>
uint32_t USwap32LongPtr(uint32_t value);                // 0x100d9bd0 USwap32<long*>
void USwapInplace32Long(int32_t& word);                 // 0x100d9c2c USwapInplace32<long>
uint32_t USwap32CharPtr(uint32_t value);                // 0x100d9c70 USwap32<char*>
}  // namespace ScriptSaveSwap
// 0x100df8fc (572 bytes): loads the dynamic cell textures (gDynamicCellTextures) and, when that works, makes the blueprint, water
// and flags meshes (CDynamicMesh); a mesh whose constructor reports failure gets the textures freed again (once in all).
void DC_SetupDynamicCellMeshes();
// 0x100e01ac (132 bytes): when the meshes exist, deletes them and frees the textures; clears the flag.
void DC_FreeDynamicCellMeshes();
extern sDTextureSet gDynamicCellTextures;   // 0x102a02fc (initialised data, defined in src/LevelLeaves.cpp)
extern int32_t gDynamicCellF9BD4;           // 0x102f9bd4  cleared first
extern uint32_t gDynamicCellsReady;         // 0x102f9bd8  1 once the meshes exist
extern uint32_t gDynamicCellRandom[110];    // 0x102f9bdc  rand() % 3 + 15 each
extern int32_t gDynamicCellF9D94;           // 0x102f9d94  0
extern float gDynamicCellF9D98;             // 0x102f9d98  0.0f
extern float gDynamicCellF9D9C;             // 0x102f9d9c  0.0f
extern float gDynamicCellF9DA0;             // 0x102f9da0  0.0f
extern int32_t gDynamicCellFBDBC;           // 0x102fbdbc  0
extern uint8_t gDynamicCellFlag;            // 0x102a031c  initially 1, set to 1
extern CDynamicMesh* gBlueprintMesh;        // 0x1034c188  "BlueprintMesh"
extern CDynamicMesh* gWaterMesh;            // 0x1034c184  "WaterMesh"
extern CDynamicMesh* gFlagsMesh;            // 0x1034c180  "FlagsMesh"

// The water's start position, three floats that CGame::InitLevel_Initialise (src/Game_level.cpp, unit w117) copies from the level's balance
// (WaterStartPos X, Y, Z); DC_TickDynamicCellMeshes (0x100dfd88, untranslated) reads them. Global 0x102f9bc8, original name unknown.
extern s3DPoint gWaterStartPos;

// STATIC_WhiteOut's four RGBA colours (doubles, 32 bytes each), at 0x102a03a4 (initialised data; defined in src/DynamicCells_STATIC_WhiteOut.cpp).
extern double gWhiteOutColors[16];
// Double-precision immediate mode GL calls STATIC_WhiteOut imports (glue 0x1023b278, 0x1023b290).
extern "C" void glColor4dv(const double* components);
extern "C" void glVertex2d(double x, double y);

static_assert(sizeof(CParkAnalyser) == 0x21c48, "CParkAnalyser is 0x21c48 bytes");
static_assert(sizeof(CHistory<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>, 3>) == 0x14, "object history is 0x14 bytes");
static_assert(offsetof(CParkAnalyser, mMapSquaresPerCell) == 0xc, "CParkAnalyser::mMapSquaresPerCell");
static_assert(offsetof(CParkAnalyser, mCells) == 0x20, "CParkAnalyser::mCells");
static_assert(offsetof(CParkAnalyser, mNearbyCells) == 0x1f020, "CParkAnalyser::mNearbyCells");
static_assert(offsetof(CParkAnalyser, mLastOfObjectType) == 0x1f068, "CParkAnalyser::mLastOfObjectType");
static_assert(offsetof(CParkAnalyser, mVal1F0B8) == 0x1f0b8, "CParkAnalyser::mVal1F0B8");
static_assert(offsetof(CParkAnalyser, mHistory1F100) == 0x1f100, "CParkAnalyser::mHistory1F100");
static_assert(offsetof(CParkAnalyser, mHistory20F10) == 0x20f10, "CParkAnalyser::mHistory20F10");
static_assert(offsetof(CParkAnalyser, mHistory21160) == 0x21160, "CParkAnalyser::mHistory21160");
static_assert(offsetof(CParkAnalyser, mHistory21600) == 0x21600, "CParkAnalyser::mHistory21600");
static_assert(offsetof(CParkAnalyser, mHistory216D8) == 0x216d8, "CParkAnalyser::mHistory216D8");
static_assert(offsetof(CParkAnalyser, mOpinionHistory) == 0x216fc, "CParkAnalyser::mOpinionHistory");
static_assert(offsetof(CParkAnalyser, mMisbehavingKids) == 0x21bfc, "CParkAnalyser::mMisbehavingKids");
static_assert(offsetof(CParkAnalyser, mLastStrike) == 0x21c0c, "CParkAnalyser::mLastStrike");
static_assert(offsetof(CParkAnalyser, mLastStaffHired) == 0x21c34, "CParkAnalyser::mLastStaffHired");
static_assert(offsetof(CParkAnalyser, mLongestStay) == 0x21c44, "CParkAnalyser::mLongestStay");
