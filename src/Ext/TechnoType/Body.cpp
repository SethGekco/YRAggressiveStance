#include "Body.h"

#include <CCINIClass.h>
#include <InfantryTypeClass.h>
#include <Helpers/Macro.h>

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceAlwaysMap;
std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceExemptMap;
std::map<TechnoTypeClass*, int>  TechnoTypeExt::AggressiveStanceTogglableMap;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static CCINIClass* GetRulesINI()
{
    return CCINIClass::INI_Rules;
}

bool TechnoTypeExt::IsAlwaysAggressiveStance(TechnoTypeClass* pType)
{
    if (!pType) return false;
    auto it = AggressiveStanceAlwaysMap.find(pType);
    if (it != AggressiveStanceAlwaysMap.end())
        return it->second;

    bool val = false;
    if (auto* pINI = GetRulesINI())
        if (pType->ID)
            val = pINI->ReadBool(pType->ID, "AggressiveStance.Always", false);

    AggressiveStanceAlwaysMap[pType] = val;
    return val;
}

bool TechnoTypeExt::IsExemptFromAggressiveStance(TechnoTypeClass* pType)
{
    if (!pType) return false;
    auto it = AggressiveStanceExemptMap.find(pType);
    if (it != AggressiveStanceExemptMap.end())
        return it->second;

    bool val = false;
    if (auto* pINI = GetRulesINI())
        if (pType->ID)
            val = pINI->ReadBool(pType->ID, "AggressiveStance.Exempt", false);

    AggressiveStanceExemptMap[pType] = val;
    return val;
}

int TechnoTypeExt::GetTogglable(TechnoTypeClass* pType)
{
    if (!pType) return -1;
    auto it = AggressiveStanceTogglableMap.find(pType);
    if (it != AggressiveStanceTogglableMap.end())
        return it->second;

    auto* pINI = GetRulesINI();
    if (!pINI || !pType->ID)
    {
        AggressiveStanceTogglableMap[pType] = -1;
        return -1;
    }

    // Check if the tag is present at all
    char buf[8] = {};
    pINI->ReadString(pType->ID, "AggressiveStance.Togglable", "", buf, sizeof(buf));
    if (buf[0] == '\0')
    {
        // Tag absent — use auto-detect
        AggressiveStanceTogglableMap[pType] = -1;
        return -1;
    }

    bool val = pINI->ReadBool(pType->ID, "AggressiveStance.Togglable", true);
    int result = val ? 1 : 0;
    AggressiveStanceTogglableMap[pType] = result;
    return result;
}
