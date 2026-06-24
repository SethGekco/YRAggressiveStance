#include <HouseClass.h>
#include <TechnoClass.h>
#include <WeaponTypeClass.h>
#include <ObjectClass.h>
#include <Commands/AggressiveStance.h>
#include <Ext/TechnoType/Body.h>
#include <Helpers/Macro.h>
#include <CCINIClass.h>

static bool PassesHealthThreshold(WeaponTypeClass* pWeapon, TechnoClass* pTarget)
{
    if (!pWeapon || !pWeapon->ID || !pTarget) return true;
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (!pINI) return true;
    double maxHealth = pINI->ReadDouble(pWeapon->ID, "CanTarget.MaxHealth", 1.0);
    double minHealth = pINI->ReadDouble(pWeapon->ID, "CanTarget.MinHealth", 0.0);
    if (maxHealth >= 1.0 && minHealth <= 0.0) return true;
    double hp = pTarget->GetHealthPercentage();
    return (hp < maxHealth) && (hp >= minHealth);
}

static bool PassesHouseFilter(WeaponTypeClass* pWeapon, TechnoClass* pThis, TechnoClass* pTarget)
{
    if (!pWeapon || !pWeapon->ID) return true;
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (!pINI) return true;
    // Read CanTargetHouses - if it contains "enemy" but target is friendly, skip this weapon
    // We do a simple check: if target owner is allied with firer, and weapon has enemy-only filter
    // Use vanilla relationship check
    if (!pThis->Owner || !pTarget->Owner) return true;
    // Read raw value - "enemy" means allies/owner not allowed
    char buf[64] = {};
    pINI->ReadString(pWeapon->ID, "CanTargetHouses", "all", buf, sizeof(buf));
    // If "enemy" is specified and target is friendly/allied, this weapon can't fire
    if (_stricmp(buf, "enemy") == 0 || _stricmp(buf, "enemies") == 0)
    {
        if (pThis->Owner->IsAlliedWith(pTarget->Owner))
            return false;
    }
    // If "allies" or "owner" and target is enemy, skip
    if (_stricmp(buf, "allies") == 0 || _stricmp(buf, "ally") == 0 || _stricmp(buf, "owner") == 0)
    {
        if (!pThis->Owner->IsAlliedWith(pTarget->Owner))
            return false;
    }
    return true;
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
            // Find a weapon that both passes house filter AND health threshold for this target
            bool anyWeaponCanFire = false;
            for (int i = 0; i < 2; i++)
            {
                auto pWeaponStruct = pThis->GetWeapon(i);
                if (!pWeaponStruct || !pWeaponStruct->WeaponType) continue;

                WeaponTypeClass* pWeapon = pWeaponStruct->WeaponType;

                // Skip weapons that can't target this house relationship
                if (!PassesHouseFilter(pWeapon, pThis, pTarget)) continue;

                // Skip weapons where target health is out of range
                if (!PassesHealthThreshold(pWeapon, pTarget)) continue;

                anyWeaponCanFire = true;
                break;
            }

            if (!anyWeaponCanFire)
                return 0;

            return 0x6F88BF;
        }
    }
    return 0;
}
