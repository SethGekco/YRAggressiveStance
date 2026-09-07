#include "Body.h"

#include <CCINIClass.h>

std::map<HouseTypeClass*, bool> HouseTypeExt::AggressiveStanceMap;

bool HouseTypeExt::IsAggressiveStance(HouseTypeClass* pType)
{
    if (!pType) return false;

    auto it = AggressiveStanceMap.find(pType);
    if (it != AggressiveStanceMap.end())
        return it->second;

    bool val = false;
    if (CCINIClass* pINI = CCINIClass::INI_Rules)
        if (pType->ID)
            val = pINI->ReadBool(pType->ID, "AggressiveStance", false);

    AggressiveStanceMap[pType] = val;
    return val;
}
