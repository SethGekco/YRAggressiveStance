#pragma once

#include <HouseTypeClass.h>
#include <map>

// Extends HouseTypeClass (a country) with the AggressiveStance country tag.
class HouseTypeExt
{
public:
    // AggressiveStance=yes  (on a [Country] section)
    // Every unit owned by a house of this country is permanently in aggressive
    // stance, like AggressiveStance.Always but applied per-country.
    static std::map<HouseTypeClass*, bool> AggressiveStanceMap;

    // Returns true if pType's country section has AggressiveStance=yes.
    static bool IsAggressiveStance(HouseTypeClass* pType);
};
