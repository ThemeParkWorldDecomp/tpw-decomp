# Unit w171: a ride script's screams, the repair particle effect, DrawPeep and two getters of rsse.cpp (src/rsse_scream.cpp). The callees outside
# the unit (the sound events, the emitters, GetNodePos, the ride database, the peep's sprite, the clock and the assertion) are recording fakes in
# tests/unit_RideScream.cpp, which also defines the globals of other files.
set(TPW_UNIT_RideScream src/rsse_scream.cpp platform/arena.cpp)
