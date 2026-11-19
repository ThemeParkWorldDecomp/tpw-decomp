// EntertainerSecondActs.h: the entertainer-second-acts mod's second acts (TPW_MOD_HOOK entertainer_second_acts).
// An entertainer's act is one of its sprite file's custom blocks (CEntertainer::SetState, src/game/people/Entertainer.cpp):
// the block names the act's action and the script that plays it. Every entertainer file the game shipped has one block,
// naming action 4, so each costume has one act. Eight costumes also have a finished second act in their sprite file that
// no block names and nothing plays: action 10 of the Halloween world's vampire, Frankenstein's monster and hunchback
// (spr_dr, spr_fk, spr_hu), of the Jungle world's dinosaur, explorer and tribal dancer (spr_di, spr_ex, spr_na) and of the
// Space world's orange alien (spr_sm), and action 11 of the Space world's purple monster (spr_mo). With the hook on, an
// act of one of them is the second act or the first, chosen at random each time an act starts, with the random number
// the original already draws there (no other draw: the park's random numbers stay the original's). The act's length,
// its sound when it ends and the crowd's reaction are the first act's. Only with the port's fixes: off (the default) or
// in a build without them, every act is the original's.
#pragma once

