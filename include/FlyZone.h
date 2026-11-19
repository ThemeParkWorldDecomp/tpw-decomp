// FlyZone.h: where a flying ride's car (the High Flyer, the blimp, the bird, the spaceship, Jurassic Tours) crosses into the
// park and out of it: a point just outside one edge of the fly zone, along that edge (EmployEric, where a car starts, and
// FLY_Tick's state 4 to 9, where a leaving car goes; src/game/rides/FlyingRide.cpp). The Mac takes labs of a random word,
// multiplies it by 100 in 32 bits and keeps the signed remainder by the edge's length, and the point is that far from the
// edge's low end. Any labs result of 21474837 or more makes the product wrap, about half of them to a negative word, and a
// negative word's signed remainder is negative: the point then lies up to a whole edge's length beyond the zone's corner,
// far past the map's edge. Header only and pure, so the host tests see what the game does.
#pragma once
