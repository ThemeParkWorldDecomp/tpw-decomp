// CUI2DMap::UpdateSpritePosition, DrawCoasterTracks and StoreMapGridDetails (ui_2dmap.cpp), unit v5200. The map object has no members yet:
// its fields are read by Mac offset (see the class in include/ui_main.h).
#include <cstdint>
#include <cstring>

#include "CellBase.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "Light.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPMain.h"
#include "TbGraphicFile.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "ui_main.h"

// Globals nobody else defines (initialised data / BSS of the Mac).
uint16_t gUI2DMapGridThingListEnd = 0;           // 0x102b2912
uint32_t gUI2DMapColourTable[0x264 / 4] = {};    // 0x103970f0

namespace {

template <typename T>
T& At(void* base, int32_t offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(base) + offset);
}

float FloatBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

const char kThingZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10251a08

}  // namespace

// 0x10230a0c (388 bytes): puts the sprite on the screen from its map place: the screen position in the sprite's +0x0c and +0x10, then
// SetPosition (x and y in -1..1 clip space, z 0.99 for the shown statuses 0 to 2 else 0.9999) and SetScale (half the zoom times the sprite's scale).
void CUI2DMap::UpdateSpritePosition(CUI2DMapSprite* sprite) {
    void* self = this;
    void* sp = sprite;
    const float mapW = static_cast<float>(At<int32_t>(self, 0x24));
    const float mapH = static_cast<float>(At<int32_t>(self, 0x28));
    // The screen size is converted as an unsigned word (no xoris on those two).
    const float screenW = static_cast<float>(static_cast<uint32_t>(gScreenWidth));
    const float screenH = static_cast<float>(static_cast<uint32_t>(gScreenHeight));
    const float perX = 480.0f / mapW;   // f7
    const float perY = 425.0f / mapH;   // f8
    const float half = 0.5f;

    float t = At<float>(sp, 4) - At<float>(self, 0x54);
    t = At<float>(self, 0x5c) * t;
    t = perX * t;
    At<float>(sp, 0x0c) = ppc_fmadds(half, screenW, t);

    t = At<float>(sp, 8) - At<float>(self, 0x58);
    t = At<float>(self, 0x5c) * t;
    t = perY * t;
    At<float>(sp, 0x10) = ppc_fmadds(half, screenH, t);

    const float two = 2.0f;
    float px = two * At<float>(sp, 0x0c);
    float py = two * At<float>(sp, 0x10);
    px = px / screenW;
    py = py / screenH;
    px = px - 1.0f;
    py = py - 1.0f;

    _D3DVECTOR position;
    position.x = px;
    position.y = py;
    const int32_t status = At<int32_t>(sp, 0x14);
    if (status < 3 && status >= 0) {
        position.z = FloatBits(0x3f7d70a4u);   // 0.99
    } else {
        position.z = FloatBits(0x3f7ff972u);   // 0.9999
    }
    sprite->SetPosition(position);

    float scale = half * At<float>(self, 0x5c);
    scale = scale * At<float>(sp, 0x24);
    sprite->SetScale(scale);
}

// 0x1022f160 (832 bytes): draws every coaster track of the park on the map as short antialiased lines between 0.1 steps of the Bezier of each
// pair of controls, in the track's colour; the line routine depends on the render area's bytes per pixel ((depth + 7) >> 3).
void CUI2DMap::DrawCoasterTracks() {
    void* self = this;
    bfl::draw::CDraw draw;
    draw.SetCurrent(HostArea());   // the Mac's area at 0x2c (ui_main.h)
    const float mapW = static_cast<float>(At<int32_t>(self, 0x24));
    const float scale = 480.0f / (10.0f * mapW);   // f31
    const uint32_t tracks = static_cast<uint32_t>(ui_GetNumCoasterTracks());
    const char* table = reinterpret_cast<const char*>(gUI2DMapColourTable);
    for (uint32_t i = 0; i < tracks; i++) {
        void* firstControl;
        tag_sBezierControl* segment = GetNthCoasterTrackStart(i, &firstControl);
        const int32_t colourIndex = ppc_fctiwz(static_cast<double>(At<float>(firstControl, 0x40)));
        const bfl::draw::CColour colour = UI2DMReturnScaleColour(
            colourIndex & 0xff, *reinterpret_cast<const bfl::draw::CColour*>(table + 0x1f8),
            *reinterpret_cast<const bfl::draw::CColour*>(table + 0x21c), *reinterpret_cast<const bfl::draw::CColour*>(table + 0x240));
        bool first = true;
        s3DPoint previous;
        previous.x = 0.0f;
        previous.y = 0.0f;
        previous.z = 0.0f;
        if (segment != nullptr) {
            while (segment->mNext != nullptr) {
                for (float t = 0.0f; t <= 1.0f; t = t + 0.1f) {
                    s3DPoint point;
                    BezierPosition(segment, segment->mNext, t, &point);
                    if (first) {
                        first = false;
                    } else {
                        const float fromX = previous.x * scale;
                        const float fromZ = previous.z * scale;
                        TbPoint from;
                        from.x = ppc_fctiwz(static_cast<double>(fromX)) + 0xf;
                        from.y = ppc_fctiwz(static_cast<double>(fromZ)) + 0x2a;
                        const float toX = point.x * scale;
                        const float toZ = point.z * scale;
                        TbPoint to;
                        to.x = ppc_fctiwz(static_cast<double>(toX)) + 0xf;
                        to.y = ppc_fctiwz(static_cast<double>(toZ)) + 0x2a;
                        const TbRect& clip = draw.GetClipRect();
                        const TbRenderArea& area = *HostArea();
                        const uint32_t kind = static_cast<uint32_t>(At<int32_t>(self, 0x3c) + 7) >> 3;
                        if (kind == 3) {
                            tpw_antialias::Antialiased<3>(tpw_intensify::CTraits<3>(), area, clip, colour, from, to);
                        } else if (kind < 3) {
                            if (kind == 1) {
                                tpw_antialias::Antialiased<1>(tpw_intensify::CTraits<1>(), area, clip, colour, from, to);
                            } else if (kind != 0) {
                                tpw_antialias::Antialiased<2>(tpw_intensify::CTraits<2>(), area, clip, colour, from, to);
                            }
                        } else if (kind < 5) {
                            tpw_antialias::Antialiased<4>(tpw_intensify::CTraits<4>(), area, clip, colour, from, to);
                        }
                    }
                    previous = point;
                }
                segment = segment->mNext;
            }
        }
    }
}

// 0x1022de40 (1104 bytes): fills the 128 x 128 grid of (kind, thing id) entries at +0x60 for the map's width x height cells: 9 hoarding, 10 path,
// 11 queue, 12 flagged (mask 0x40), and for ride / entrance / exit / track / pylon cells the kind of the first thing of the parent cell's list
// that is an object (3 for a database kind of 2, 1 or 8 for 0, 2 for 1, 4 to 7 by the object's flags for 3), with the thing's id at +4.
void CUI2DMap::StoreMapGridDetails() {
    void* self = this;
    char* world = reinterpret_cast<char*>(gGameWorld);
    char* cells = world + 0x2d8;
    for (int32_t y = 0; y < At<int32_t>(self, 0x28); y++) {
        for (int32_t x = 0; x < At<int32_t>(self, 0x24); x++) {
            char* slot = nullptr;
            if (x >= 0 && x < 0x80 && y >= 0 && y < 0x80) {
                slot = static_cast<char*>(self) + (x + y * 0x80) * 8 + 0x60;
            }
            At<int32_t>(slot, 0) = 0;
            At<uint16_t>(slot, 4) = 0;
            const uint16_t cellId = static_cast<uint16_t>(x + y * 0x80 + 1);
            CMapCell* cell = reinterpret_cast<CMapCell*>(cells + (static_cast<int32_t>(cellId) - 1) * 0x44);
            if (cell->IsHoarding() != 0) {
                At<int32_t>(slot, 0) = 9;
            } else if (cell->IsRide() != 0 || cell->IsEntrance() != 0 || cell->IsExit() != 0 || cell->IsTrack() != 0 ||
                       cell->IsCoasterPylon() != 0) {
                uint16_t parent;
                if (cell->IsTrack() != 0) {
                    CTrackCell* track = cell->GetTrackCellEquiv();
                    if (track->IsTrackX() != 0) {
                        const int32_t trackId = At<uint16_t>(track, 0x10);
                        track = reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + (trackId - 1) * 0x28 + 0x110000);
                    }
                    parent = At<uint16_t>(track, 0x10);
                } else {
                    parent = At<uint16_t>(cell, 0x10);
                }
                bool found = false;
                uint16_t thingId = At<uint16_t>(cells + (static_cast<int32_t>(parent) - 1) * 0x44, 0x24);
                while (thingId != gUI2DMapGridThingListEnd && !found) {
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMessage);
                    if (static_cast<CThing*>(gThingArray[thingId].mThing)->mKind == 3) {
                        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMessage);
                        CObject* object = static_cast<CObject*>(gThingArray[thingId].mThing);
                        const int32_t databaseKind = object->GetDBInfo()->mUIType;   // the Mac's +0x7a8, by name
                        if (databaseKind == 2) {
                            found = true;
                            At<int32_t>(slot, 0) = 3;
                            At<uint16_t>(slot, 4) = thingId;
                        } else if (databaseKind < 2) {
                            if (databaseKind == 0) {
                                if (cell->IsRide() == 0 && (cell->IsTrack() != 0 || cell->IsCoasterPylon() != 0)) {
                                    At<int32_t>(slot, 0) = 8;
                                } else {
                                    At<int32_t>(slot, 0) = 1;
                                }
                                found = true;
                                At<uint16_t>(slot, 4) = thingId;
                            } else if (databaseKind >= 0) {
                                found = true;
                                At<int32_t>(slot, 0) = 2;
                                At<uint16_t>(slot, 4) = thingId;
                            }
                        } else if (databaseKind < 4) {
                            const uint16_t flags = At<uint16_t>(object, 0x2e);
                            if ((flags & 1) != 0 || (flags & 0x40) != 0) {
                                At<int32_t>(slot, 0) = 5;
                            } else if ((flags & 0x10) != 0) {
                                At<int32_t>(slot, 0) = 6;
                            } else if ((flags & 2) != 0) {
                                At<int32_t>(slot, 0) = 7;
                            } else {
                                At<int32_t>(slot, 0) = 4;
                            }
                            found = true;
                            At<uint16_t>(slot, 4) = thingId;
                        }
                    }
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMessage);
                    thingId = At<uint16_t>(gThingArray[thingId].mThing, 0xa);
                }
            } else if (cell->IsPath() != 0) {
                At<int32_t>(slot, 0) = 10;
            } else if (cell->IsQueue() != 0) {
                At<int32_t>(slot, 0) = 11;
            } else if (cell->IsFlag(0x40) != 0) {
                At<int32_t>(slot, 0) = 12;
            }
        }
    }
}
