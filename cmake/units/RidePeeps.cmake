# Unit w149: the peep tables of a ride script (WALKNODE_Tick, Trigger, Get, Float, Release; BOUNCE_Put, Get, ForceGet, Tick; LIMBO_Put, Get,
# ForceGet; GetNodePos; src/rsse_peeps.cpp). The callees outside the unit (the peep drawing, the embedded-node lookups, the clock, the ride heads,
# the emitter, the assertion and the peep's sprite) are recording fakes in tests/unit_RidePeeps.cpp, which also defines the globals of other files.
set(TPW_UNIT_RidePeeps src/rsse_peeps.cpp platform/arena.cpp)
