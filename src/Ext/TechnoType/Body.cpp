#include "Body.h"

#include <TechnoTypeClass.h>
#include <CCINIClass.h>

std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceAlwaysMap;

bool TechnoTypeExt::IsAlwaysAggressiveStance(TechnoTypeClass* pType)
{
    auto it = TechnoTypeExt::AggressiveStanceAlwaysMap.find(pType);
    if (it != TechnoTypeExt::AggressiveStanceAlwaysMap.end())
        return it->second;

    // Lazy read from rules INI on first encounter
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (pINI && pType && pType->ID)
    {
        bool val = pINI->ReadBool(pType->ID, "AggressiveStance.Always", false);
        TechnoTypeExt::AggressiveStanceAlwaysMap[pType] = val;
        return val;
    }

    TechnoTypeExt::AggressiveStanceAlwaysMap[pType] = false;
    return false;
}
