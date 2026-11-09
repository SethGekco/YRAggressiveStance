#include "Body.h"

#include <CCINIClass.h>
#include <Ext/TechnoType/Body.h>

std::map<HouseTypeClass*, bool> HouseTypeExt::AggressiveStanceMap;

bool HouseTypeExt::ReadAggressiveStance(HouseTypeClass* pType)
{
    bool val = false;
    if (CCINIClass* pINI = CCINIClass::INI_Rules)
        if (pType->ID)
            val = pINI->ReadBool(pType->ID, "AggressiveStance", false);
    return val;
}

bool HouseTypeExt::IsAggressiveStance(HouseTypeClass* pType)
{
    if (!pType) return false;

    if (PrewarmVerify::On)
        PrewarmVerify::Lookup();

    auto it = AggressiveStanceMap.find(pType);
    if (it != AggressiveStanceMap.end())
        return it->second;

    // A country made after the last prewarm: read it now, as before.
    PrewarmVerify::Miss("AggressiveStance (country)", pType->ID);
    bool const val = ReadAggressiveStance(pType);
    AggressiveStanceMap[pType] = val;
    return val;
}

void HouseTypeExt::Prewarm()
{
    AggressiveStanceMap.clear();

    for (int i = 0; i < HouseTypeClass::Array->Count; i++)
    {
        HouseTypeClass* pType = HouseTypeClass::Array->Items[i];
        if (pType)
            AggressiveStanceMap[pType] = ReadAggressiveStance(pType);
    }
}
