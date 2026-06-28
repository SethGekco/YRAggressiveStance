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
// Read once from INI on first encounter. Default: max=1.0, min=0.0 (no filter).
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

// Returns true if pTarget's health passes the weapon's health filter.
static bool PassesHealthFilter(WeaponTypeClass* pWeapon, TechnoClass* pTarget)
{
    if (!pWeapon || !pTarget) return true;
    const auto& filter = GetWeaponHealthFilter(pWeapon);
    if (!filter.HasFilter) return true;
    double hp = pTarget->GetHealthPercentage();
    return (hp < filter.MaxHealth) && (hp >= filter.MinHealth);
}

// Returns true if the weapon can target this house relationship.
// Reads CanTargetHouses from INI - handles common values only.
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

// ---------------------------------------------------------------------------
// Hook 1: 0x6F858F - Unarmed building gate (AggressiveStance bypass)
// Fires when vanilla would deny a building because it has ThreatPosed=0.
// We allow it through if the unit is in aggressive stance AND a weapon
// passes both house and health filters for this target.
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F858F, TechnoClass_EvaluateObject_AggressiveStance, 0x7)
{
    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pThis->Owner->IsControlledByHuman()
        && pTarget && pTarget->WhatAmI() == AbstractType::Building)
    {
        bool isAggressive = AggressiveStanceClass::AggressiveStanceMap[pThis]
            || TechnoTypeExt::IsAlwaysAggressiveStance(pThis->GetTechnoType())
            || (pThis->Transporter && (AggressiveStanceClass::AggressiveStanceMap[pThis->Transporter]
                || TechnoTypeExt::IsAlwaysAggressiveStance(pThis->Transporter->GetTechnoType())));

        if (isAggressive)
        {
            for (int i = 0; i < 2; i++)
            {
                auto pWeaponStruct = pThis->GetWeapon(i);
                if (!pWeaponStruct || !pWeaponStruct->WeaponType) continue;
                WeaponTypeClass* pWeapon = pWeaponStruct->WeaponType;
                if (!PassesHouseFilter(pWeapon, pThis, pTarget)) continue;
                if (!PassesHealthFilter(pWeapon, pTarget)) continue;
                return 0x6F88BF;
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Hook 2: 0x6F8604 - CanTarget allow point (all buildings)
// This is where ALL buildings end up when EvaluateObject considers them valid,
// including armed ones with ThreatPosed > 0 that never hit hook 1.
// We enforce CanTarget.MaxHealth / CanTarget.MinHealth here for any building
// target, since Phobos's CanFire hook does not enforce these for buildings.
// pThis = EDI, pTarget = ESI (same as hook 1).
// Stolen bytes: 8A 44 24 13 84 C0 (6 bytes).
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F8604, TechnoClass_EvaluateObject_BuildingHealthFilter, 0x6)
{
    enum { Deny = 0x6F894F };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Building)
    {
        // Find the weapon that would fire at this target and check its health filter.
        // If no weapon can fire (all filtered), deny the target.
        bool anyWeaponPasses = false;
        for (int i = 0; i < 2; i++)
        {
            auto pWeaponStruct = pThis->GetWeapon(i);
            if (!pWeaponStruct || !pWeaponStruct->WeaponType) continue;
            WeaponTypeClass* pWeapon = pWeaponStruct->WeaponType;
            if (!PassesHouseFilter(pWeapon, pThis, pTarget)) continue;
            if (!PassesHealthFilter(pWeapon, pTarget)) continue;
            anyWeaponPasses = true;
            break;
        }

        if (!anyWeaponPasses)
            return Deny;
    }

    return 0;
}
