#include <HouseClass.h>
#include <TechnoClass.h>
#include <WeaponTypeClass.h>
#include <ObjectClass.h>
#include <CCINIClass.h>
#include <Commands/AggressiveStance.h>
#include <Ext/TechnoType/Body.h>
#include <Helpers/Macro.h>
#include <map>

struct WeaponHealthFilter
{
    double MaxHealth { 1.0 };
    double MinHealth { 0.0 };
    bool   HasFilter { false };
};

static std::map<WeaponTypeClass*, WeaponHealthFilter> WeaponHealthFilterCache;

static const WeaponHealthFilter& GetWeaponHealthFilter(WeaponTypeClass* pWeapon)
{
    auto it = WeaponHealthFilterCache.find(pWeapon);
    if (it != WeaponHealthFilterCache.end())
        return it->second;

    WeaponHealthFilter filter;
    if (CCINIClass* pINI = CCINIClass::INI_Rules)
    {
        if (pWeapon && pWeapon->ID)
        {
            filter.MaxHealth = pINI->ReadDouble(pWeapon->ID, "CanTarget.MaxHealth", 1.0);
            filter.MinHealth = pINI->ReadDouble(pWeapon->ID, "CanTarget.MinHealth", 0.0);
            filter.HasFilter = (filter.MaxHealth < 1.0 || filter.MinHealth > 0.0);
        }
    }

    return WeaponHealthFilterCache.emplace(pWeapon, filter).first->second;
}

static bool PassesHealthFilter(WeaponTypeClass* pWeapon, TechnoClass* pTarget)
{
    if (!pWeapon || !pTarget) return true;
    const auto& filter = GetWeaponHealthFilter(pWeapon);
    if (!filter.HasFilter) return true;
    double hp = pTarget->GetHealthPercentage();
    return (hp < filter.MaxHealth) && (hp >= filter.MinHealth);
}

static bool PassesHouseFilter(WeaponTypeClass* pWeapon, TechnoClass* pThis, TechnoClass* pTarget)
{
    if (!pWeapon || !pWeapon->ID || !pThis->Owner || !pTarget->Owner) return true;
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (!pINI) return true;
    char buf[32] = {};
    pINI->ReadString(pWeapon->ID, "CanTargetHouses", "all", buf, sizeof(buf));
    bool allied = pThis->Owner->IsAlliedWith(pTarget->Owner);
    if (allied && (_stricmp(buf, "enemy") == 0 || _stricmp(buf, "enemies") == 0))
        return false;
    if (!allied && (_stricmp(buf, "allies") == 0 || _stricmp(buf, "ally") == 0
        || _stricmp(buf, "owner") == 0 || _stricmp(buf, "self") == 0))
        return false;
    return true;
}

static bool IsAggressiveStance(TechnoClass* pThis)
{
    return AggressiveStanceClass::AggressiveStanceMap[pThis]
        || TechnoTypeExt::IsAlwaysAggressiveStance(pThis->GetTechnoType())
        || (pThis->Transporter && (AggressiveStanceClass::AggressiveStanceMap[pThis->Transporter]
            || TechnoTypeExt::IsAlwaysAggressiveStance(pThis->Transporter->GetTechnoType())));
}

static bool AnyWeaponCanTarget(TechnoClass* pThis, TechnoClass* pTarget)
{
    for (int i = 0; i < 2; i++)
    {
        auto pWeaponStruct = pThis->GetWeapon(i);
        if (!pWeaponStruct || !pWeaponStruct->WeaponType) continue;
        WeaponTypeClass* pWeapon = pWeaponStruct->WeaponType;
        if (!PassesHouseFilter(pWeapon, pThis, pTarget)) continue;
        if (!PassesHealthFilter(pWeapon, pTarget)) continue;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Hook 1: 0x6F858F - ThreatPosed=0 BUILDING gate (AggressiveStance bypass)
// Stolen bytes: 85 FF 74 18 8A 47 14 (7 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F858F, TechnoClass_EvaluateObject_AggressiveStance_Buildings, 0x7)
{
    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pThis->Owner->IsControlledByHuman()
        && pTarget && pTarget->WhatAmI() == AbstractType::Building)
    {
        if (IsAggressiveStance(pThis) && AnyWeaponCanTarget(pThis, pTarget))
            return 0x6F88BF;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Hook 2: 0x6F8503 - ThreatPosed=0 gate for NON-BUILDING/NON-VEHICLE targets
// Context: SUB EAX,ECX / TEST EAX,EAX / JE deny
// Stolen bytes: 2B C1 85 C0 0F 84 (6 bytes)
// If aggressive stance, jump to 0x6F851C to skip both deny checks.
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F8503, TechnoClass_EvaluateObject_AggressiveStance_Units, 0x6)
{
    enum { SkipDeny = 0x6F851C };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pThis->Owner->IsControlledByHuman()
        && pTarget && pTarget->WhatAmI() != AbstractType::Building)
    {
        if (IsAggressiveStance(pThis) && AnyWeaponCanTarget(pThis, pTarget))
            return SkipDeny;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 3: 0x6F8604 - CanTarget allow point for ALL buildings (armed + unarmed)
// Enforces CanTarget.MaxHealth/MinHealth for buildings since Phobos CanFire
// does not enforce these tags for BuildingClass targets.
// Stolen bytes: 8A 44 24 13 84 C0 (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F8604, TechnoClass_EvaluateObject_BuildingHealthFilter, 0x6)
{
    enum { Deny = 0x6F894F };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Building)
    {
        if (!AnyWeaponCanTarget(pThis, pTarget))
            return Deny;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 4: 0x6F84A9 - ThreatPosed gate for VEHICLE targets (UnitClass=0xF)
// Context: TEST CL,CL / JNZ deny - CL is ThreatPosed-derived bool for units
// If aggressive stance, skip to 0x6F84B1 (past the deny jump).
// Stolen bytes: 84 C9 0F 85 9E 04 (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F84A9, TechnoClass_EvaluateObject_AggressiveStance_Vehicles, 0x6)
{
    enum { SkipDeny = 0x6F84B1 };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pThis->Owner->IsControlledByHuman()
        && pTarget && pTarget->WhatAmI() == AbstractType::Unit)
    {
        if (IsAggressiveStance(pThis) && AnyWeaponCanTarget(pThis, pTarget))
            return SkipDeny;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 5: 0x6F84B1 - Vehicle path health filter
// Fires immediately after the vehicle ThreatPosed deny is skipped (or passed).
// EDI=pThis, ESI=pTarget still valid here.
// Enforces CanTarget.MaxHealth/MinHealth for vehicle targets since Phobos
// CanFire hook does not enforce these for non-TechnoClass cast targets.
// Stolen bytes: 8B 87 1C 02 00 00 (6 bytes) = MOV EAX,[EDI+0x21C]
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F84B1, TechnoClass_EvaluateObject_VehicleHealthFilter, 0x6)
{
    enum { Deny = 0x6F894F };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Unit)
    {
        if (!AnyWeaponCanTarget(pThis, pTarget))
            return Deny;
    }

    return 0;
}
