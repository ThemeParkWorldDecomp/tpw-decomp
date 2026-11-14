// TpiGenerations.h: the TPI Worlds and Rides mod's three generations of visitors (TPW_MOD_HOOK tpi_worlds): in the
// mod's worlds a visitor acts as Theme Park Inc's visitor of its look's generation (kid, parent or grandparent,
// include/TpiGuests.h), not only looks like one.
// Theme Park Inc's levels/Standard.sam has 24 visitor types, 8 a generation (PeepTypes[i].Generation 0, 1, 2), each with
// a preferred excitement, a starting cash, a boredom threshold and a speed (SpeedMultiplier), and per generation the
// effects of the four kinds of weather on happiness, thirst and hunger (Peep*WeatherEffects[generation].Heatwave, Rain,
// Storm, Snow), applied every PeepInfo.WeatherModifierFrequency days. Theme Park World's has 8 types, no generations, no
// speeds and no weather effects. The converter copies those lines of the player's own copy into each of the mod's
// worlds as TpiPeeps.sam (assets/tpi_convert.cpp, TpiPeepsSam), with the world's own heatwave quality
// (WeatherEffects.QualityForHeatWaveLow); CWorld::SetupNewLevel reads it (TpiParsePeeps) into gTpiPeeps.
// A visitor's type is a fixed spread of its thing index among its generation's types (TpiPeepTypeOf), so no random
// number is drawn, the Mac's own type (CPerson +0x1f0) stays as it is, and the visitor keeps its type after a park is
// loaded (the thing index is saved). Where the game reads a type's value, the mod's worlds read the Theme Park Inc
// type's: the starting cash with PeepInfo.StartingCashVarPc's spread from the Mac's own draw (CPerson::CPerson), the
// preferred excitement (CPerson::GetPreferredExcitement), the speed (CPeep::UpdateTurn) and the weather (CParkTime::Update,
// TpiWeatherDue). Theme Park World reads no BoredomThreshold anywhere, so the type's is read here but used nowhere.
// Header only and pure, so the host test sees what the game does.
#pragma once

