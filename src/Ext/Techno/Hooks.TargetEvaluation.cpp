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

// Returns true if pThis is in aggressive stance for any reason:
//   - Human player toggled via hotkey (AggressiveStanceMap entry = true)
//   - TechnoType has AggressiveStance.Always=yes
//   - TeamType has AggressiveStance=yes (AI and human teams, seeded by
//     TeamClass::AddMember hook in Ext/TeamType/Body.cpp)
//   - Transporter has any of the above
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

// Returns true if pTarget has AggressiveStance.Exempt=yes — meaning it should
// never be targeted via the aggressive stance ThreatPosed=0 bypass.
// Vanilla targeting for these types is unaffected.
static bool IsExemptTarget(TechnoClass* pTarget)
{
    if (!pTarget) return false;
    return TechnoTypeExt::IsExemptFromAggressiveStance(pTarget->GetTechnoType());
}

static bool AnyWeaponCanTarget(TechnoClass* pThis, TechnoClass* pTarget)
{
    for (int i = 0; i < 2; i++)
    {
        auto pWeaponStruct = pThis->GetWeapon(i);
        if (!pWeaponStruct || !pWeaponStruct->WeaponType) continue;
        auto* pWeapon = pWeaponStruct->WeaponType;
        if (!PassesHouseFilter(pWeapon, pThis, pTarget)) continue;
        if (!PassesHealthFilter(pWeapon, pTarget)) continue;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Hook 1: 0x6F858F - ThreatPosed=0 BUILDING gate (AggressiveStance bypass)
// Covers both human (hotkey/Always) and AI (TeamType via map entry).
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

        if (IsAggressiveStance(pThis) && AnyWeaponCanTarget(pThis, pTarget))
            return 0x6F88BF;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Hook 2: 0x6F8503 - ThreatPosed=0 gate for NON-BUILDING/NON-VEHICLE targets
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

        if (IsAggressiveStance(pThis) && AnyWeaponCanTarget(pThis, pTarget))
            return SkipDeny;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 3: 0x6F8604 - CanTarget allow point for ALL buildings (armed + unarmed)
// Enforces CanTarget.MaxHealth/MinHealth since Phobos CanFire doesn't cover
// BuildingClass targets.
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
// Stolen bytes: 84 C9 0F 85 9E 04 (6 bytes)
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F84A9, TechnoClass_EvaluateObject_AggressiveStance_Vehicles, 0x6)
{
    enum { SkipDeny = 0x6F84B1 };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Unit)
    {
        if (IsExemptTarget(pTarget))
            return 0;

        if (IsAggressiveStance(pThis) && AnyWeaponCanTarget(pThis, pTarget))
            return SkipDeny;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 5: 0x6F84B1 - Health filter for VEHICLE targets
// EDI=pThis, ESI=pTarget still valid here.
// Stolen bytes: 8B 87 1C 02 00 00 (6 bytes)
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

// ---------------------------------------------------------------------------
// Hook 6: 0x6F851C - Health filter for INFANTRY, AIRCRAFT, and other types
// Convergence point after generic ThreatPosed gate for non-building/vehicle.
// EDI=pThis, ESI=pTarget still valid here.
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
        if (!AnyWeaponCanTarget(pThis, pTarget))
            return Deny;
    }

    return 0;
}
