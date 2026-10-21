// CWorldSavvy: GetWorld's inline copy (0x1014205c, 12 bytes, src/game/ui/ui_visitorStatus.cpp, unit w129) answers *gWorld and ignores `this`.
#pragma once
#include "World.h"
class CWorldSavvy {
public:
    CWorld& GetWorld() const;
    CMap* GetMap() const;   // 0x10142098 (12 bytes, untranslated): the world's map (CWorld::GetMap)
};
