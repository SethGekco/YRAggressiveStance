#include "Body.h"

#include <Helpers/Macro.h>
#include <TechnoClass.h>
#include <TechnoTypeClass.h>
#include <CCINIClass.h>
#include <RulesClass.h>
#include <Commands/AggressiveStance.h>

std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceAlwaysMap;

bool TechnoTypeExt::IsAlwaysAggressiveStance(TechnoTypeClass* pType)
{
    auto it = TechnoTypeExt::AggressiveStanceAlwaysMap.find(pType);
    if (it != TechnoTypeExt::AggressiveStanceAlwaysMap.end())
        return it->second;

    // Not cached yet - read from RulesIni now and cache it
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

// Hook: TechnoClass::Put (0x70D0D0)
// Fires when a unit is placed on the map.
// ESI = TechnoClass* (thiscall via PUSH ESI; MOV ESI,ECX pattern - ECX = this)
// Actually: 53 56 8B F1 = PUSH EBX; PUSH ESI; MOV ESI,ECX so ECX has this at entry
// We steal 6 bytes: 53 56 8B F1 57 8D

DEFINE_HOOK(0x70D0D0, TechnoClass_Put_AggressiveStanceAlways, 0x6)
{
    GET(TechnoClass*, pThis, ECX);

    if (pThis)
    {
        TechnoTypeClass* pType = pThis->GetTechnoType();
        if (pType && TechnoTypeExt::IsAlwaysAggressiveStance(pType))
        {
            AggressiveStanceClass::AggressiveStanceMap[pThis] = true;
        }
    }

    return 0;
}
