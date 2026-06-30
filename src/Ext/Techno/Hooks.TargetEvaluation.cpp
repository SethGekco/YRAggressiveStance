#include <HouseClass.h>
#include <TechnoClass.h>
#include <WeaponTypeClass.h>
#include <ObjectClass.h>
#include <CCINIClass.h>
#include <Commands/AggressiveStance.h>
#include <Ext/TechnoType/Body.h>
#include <Helpers/Macro.h>
#include <map>

// ---------------------------------------------------------------------------
// Per-weapon cache for CanTarget.MaxHealth / CanTarget.MinHealth.
// Only ever used to DENY a target when a tag explicitly restricts it.
// Never used to deny normal vanilla targeting.
// ---------------------------------------------------------------------------

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

// Returns true (passes) unless a weapon EXPLICITLY restricts via
// CanTarget.MaxHealth/MinHealth and the target falls outside that range.
static bool PassesHealthFilter(WeaponTypeClass* pWeapon, TechnoClass* pTarget)
{
    if (!pWeapon || !pTarget) return true;
    const auto& filter = GetWeaponHealthFilter(pWeapon);
    if (!filter.HasFilter) return true; // no tag present - never deny
    double hp = pTarget->GetHealthPercentage();
    return (hp < filter.MaxHealth) && (hp >= filter.MinHealth);
}

// Returns true if pThis is in aggressive stance for any reason.
static bool IsAggressiveStance(TechnoClass* pThis)
{
    if (!pThis) return false;

    if (AggressiveStanceClass::AggressiveStanceMap[pThis])
        return true;
    if (TechnoTypeExt::IsAlwaysAggressiveStance(pThis->GetTechnoType()))
        return true;

    if (pThis->Transporter)
    {
        if (AggressiveStanceClass::AggressiveStanceMap[pThis->Transporter])
            return true;
        if (TechnoTypeExt::IsAlwaysAggressiveStance(pThis->Transporter->GetTechnoType()))
            return true;
    }

    return false;
}

// Returns true if pTarget has AggressiveStance.Exempt=yes.
static bool IsExemptTarget(TechnoClass* pTarget)
{
    if (!pTarget) return false;
    return TechnoTypeExt::IsExemptFromAggressiveStance(pTarget->GetTechnoType());
}

// Returns true unless EVERY weapon on pThis explicitly forbids this target
// via CanTarget.MaxHealth/MinHealth. A weapon with no health tag always
// passes. This is only used to gate the AGGRESSIVE STANCE bypass and the
// supplementary health-enforcement hooks - never normal vanilla targeting.
static bool AnyWeaponPassesHealthFilter(TechnoClass* pThis, TechnoClass* pTarget)
{
    bool sawAnyWeapon = false;
    for (int i = 0; i < 2; i++)
    {
        auto pWeaponStruct = pThis->GetWeapon(i);
        if (!pWeaponStruct || !pWeaponStruct->WeaponType) continue;
        sawAnyWeapon = true;
        if (PassesHealthFilter(pWeaponStruct->WeaponType, pTarget))
            return true;
    }
    // If the unit has no weapons at all, don't deny - let vanilla decide.
    if (!sawAnyWeapon) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Hook 1: 0x6F858F - ThreatPosed=0 BUILDING gate (AggressiveStance bypass)
// Only fires for buildings that vanilla would otherwise REJECT (ThreatPosed=0).
// Normal armed-building targeting never reaches this hook's deny path because
// it returns 0 (fall through to vanilla) unless aggressive stance applies.
// Stolen bytes: 85 FF 74 18 8A 47 14 (7 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F858F, TechnoClass_EvaluateObject_AggressiveStance_Buildings, 0x7)
{
    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Building)
    {
        if (IsExemptTarget(pTarget))
            return 0;

        if (IsAggressiveStance(pThis) && AnyWeaponPassesHealthFilter(pThis, pTarget))
            return 0x6F88BF;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Hook 2: 0x6F8503 - ThreatPosed=0 gate for NON-BUILDING targets
// Same logic: only intervenes for the aggressive stance bypass.
// Stolen bytes: 2B C1 85 C0 0F 84 (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F8503, TechnoClass_EvaluateObject_AggressiveStance_Units, 0x6)
{
    enum { SkipDeny = 0x6F851C };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() != AbstractType::Building)
    {
        if (IsExemptTarget(pTarget))
            return 0;

        if (IsAggressiveStance(pThis) && AnyWeaponPassesHealthFilter(pThis, pTarget))
            return SkipDeny;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 3: 0x6F8604 - CanTarget allow point for ALL buildings.
// ONLY denies if a weapon explicitly sets CanTarget.MaxHealth/MinHealth AND
// the target fails it. Units with no such tag are never affected.
// Stolen bytes: 8A 44 24 13 84 C0 (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F8604, TechnoClass_EvaluateObject_BuildingHealthFilter, 0x6)
{
    enum { Deny = 0x6F894F };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Building)
    {
        if (!AnyWeaponPassesHealthFilter(pThis, pTarget))
            return Deny;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 4: 0x6F84A9 - ThreatPosed gate for VEHICLE targets (UnitClass=0xF)
// Stolen bytes: 84 C9 0F 85 9E 04 (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F84A9, TechnoClass_EvaluateObject_AggressiveStance_Vehicles, 0x8)
{
    enum { SkipDeny = 0x6F84B1 };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Unit)
    {
        if (IsExemptTarget(pTarget))
            return 0;

        if (IsAggressiveStance(pThis) && AnyWeaponPassesHealthFilter(pThis, pTarget))
            return SkipDeny;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 5: 0x6F84B1 - Health filter for VEHICLE targets only.
// Stolen bytes: 8B 87 1C 02 00 00 (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F84B1, TechnoClass_EvaluateObject_VehicleHealthFilter, 0x6)
{
   
    return 0;
}

// ---------------------------------------------------------------------------
// Hook 6: 0x6F851C - Health filter for INFANTRY, AIRCRAFT, and other types.
// Stolen bytes: A1 30 B2 A8 00 6A (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F851C, TechnoClass_EvaluateObject_OtherHealthFilter, 0x6)
{
    enum { Deny = 0x6F894F };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget
        && pTarget->WhatAmI() != AbstractType::Building
        && pTarget->WhatAmI() != AbstractType::Unit)
    {
        if (!AnyWeaponPassesHealthFilter(pThis, pTarget))
            return Deny;
    }

    return 0;
}
