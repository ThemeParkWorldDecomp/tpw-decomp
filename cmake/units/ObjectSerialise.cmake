# Unit w229: the object serializer and its enum instances. All outside calls are recording fakes.
set(TPW_UNIT_ObjectSerialise src/game/Object.cpp)
set(TPW_UNIT_ObjectSerialise_KEEP
  __Z7LOADVARIN7CObject11ObjectStateEEiPvPT_PKci   # int LOADVAR<CObject::ObjectState>(void*, CObject::ObjectState*, char const*, int)
  __Z7SAVEVARIN7CObject11ObjectStateEEiPvPT_PKci   # int SAVEVAR<CObject::ObjectState>(void*, CObject::ObjectState*, char const*, int)
  __ZN7CObject9SerialiseEPviij   # CObject::Serialise(void*, int, int, unsigned int)
  _gObjectStaticC360
  _gObjectStaticC380
  _gObjectStaticC384
)
