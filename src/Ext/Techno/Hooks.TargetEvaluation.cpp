#include <HouseClass.h>
#include <TechnoClass.h>
#include <WeaponTypeClass.h>
#include <ObjectClass.h>
#include <CCINIClass.h>
#include <Commands/AggressiveStance.h>
#include <Ext/TechnoType/Body.h>
#include <Helpers/Macro.h>
#include <map>

// Per-weapon cache for CanTarget.MaxHealth and CanTarget.MinHealth.
// Read once from INI on first encounter, never re-read.
// Default: MaxHealth=1.0 (no restriction), MinHealth=0.0 (no restriction).
struct WeaponHealthFilter
{
    double MaxHealth = 1.0;
    double MinHealth = 0.0;
    bool   HasFilter = false; // true if either tag was non-default
};

static std::map<WeaponTypeClass*, WeaponHealthFilter> WeaponHealthFilterCache;

static const WeaponHealthFilter& GetWeaponHealthFilter(WeaponTypeClass* pWeapon)
{
    auto it = WeaponHealthFilterCache.find(pWeapon);
    if (it != WeaponHealthFilterCache.end())
        return it->second;

    WeaponHealthFilter filter;
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (pINI && pWeapon && pWeapon->ID)
    {
        filter.MaxHealth = pINI->ReadDouble(pWeapon->ID, "CanTarget.MaxHealth", 1.0);
        filter.MinHealth = pINI->ReadDouble(pWeapon->ID, "CanTarget.MinHealth", 0.0);
        filter.HasFilter = (filter.MaxHealth < 1.0 || filter.MinHealth > 0.0);
    }

    WeaponHealthFilterCache[pWeapon] = filter;
    return WeaponHealthFilterCache[pWeapon];
}

// Returns true if pTarget's health is within the weapon's allowed range.
static bool PassesHealthFilter(WeaponTypeClass* pWeapon, TechnoClass* pTarget)
{
    if (!pWeapon || !pTarget) return true;

    const auto& filter = GetWeaponHealthFilter(pWeapon);
    if (!filter.HasFilter) return true;

    double hp = pTarget->GetHealthPercentage();
    return (hp < filter.MaxHealth) && (hp >= filter.MinHealth);
}

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
            // Find the best weapon this unit would use against this target
            // and check CanTarget.MaxHealth / CanTarget.MinHealth.
            // We do this here because Phobos's CanFire hook does not enforce
            // these tags for BuildingClass targets (pTargetTechno is null there).
            // Check both weapons; use the first one that passes the house filter.
            bool anyWeaponPasses = false;
            for (int i = 0; i < 2; i++)
            {
                auto pWeaponStruct = pThis->GetWeapon(i);
                if (!pWeaponStruct || !pWeaponStruct->WeaponType) continue;

                WeaponTypeClass* pWeapon = pWeaponStruct->WeaponType;

                // Skip weapons that explicitly can't target this house relationship.
                // Read CanTargetHouses from INI directly since we can't access Phobos ext.
                CCINIClass* pINI = CCINIClass::INI_Rules;
                if (pINI && pWeapon->ID)
                {
                    char buf[32] = {};
                    pINI->ReadString(pWeapon->ID, "CanTargetHouses", "all", buf, sizeof(buf));
                    bool allied = pThis->Owner->IsAlliedWith(pTarget->Owner);
                    // If weapon is enemy-only and target is friendly, skip
                    if (allied && (_stricmp(buf, "enemy") == 0 || _stricmp(buf, "enemies") == 0))
                        continue;
                    // If weapon is ally/owner-only and target is enemy, skip
                    if (!allied && (_stricmp(buf, "allies") == 0 || _stricmp(buf, "ally") == 0
                        || _stricmp(buf, "owner") == 0 || _stricmp(buf, "self") == 0))
                        continue;
                }

                // Check health filter for this weapon
                if (!PassesHealthFilter(pWeapon, pTarget)) continue;

                anyWeaponPasses = true;
                break;
            }

            if (!anyWeaponPasses)
                return 0;

            return 0x6F88BF;
        }
    }
    return 0;
}
