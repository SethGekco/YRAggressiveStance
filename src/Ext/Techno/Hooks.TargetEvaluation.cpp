#include <HouseClass.h>
#include <TechnoClass.h>
#include <BuildingClass.h>
#include <WeaponTypeClass.h>
#include <ObjectClass.h>
#include <Commands/AggressiveStance.h>
#include <Ext/TechnoType/Body.h>
#include <Helpers/Macro.h>
#include <CCINIClass.h>

// Read CanTarget.MaxHealth from rules INI for a given weapon type.
// Returns 1.0 (no restriction) if the tag is absent.
static double GetCanTargetMaxHealth(WeaponTypeClass* pWeapon)
{
    if (!pWeapon || !pWeapon->ID) return 1.0;
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (!pINI) return 1.0;
    return pINI->ReadDouble(pWeapon->ID, "CanTarget.MaxHealth", 1.0);
}

static double GetCanTargetMinHealth(WeaponTypeClass* pWeapon)
{
    if (!pWeapon || !pWeapon->ID) return 0.0;
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (!pINI) return 0.0;
    return pINI->ReadDouble(pWeapon->ID, "CanTarget.MinHealth", 0.0);
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
            // Honor CanTarget.MaxHealth / CanTarget.MinHealth on both weapons
            // Use the same health ratio method Phobos uses: GetHealthPercentage()
            auto pType = pThis->GetTechnoType();
            if (pType)
            {
                // Check primary then secondary weapon
                for (int i = 0; i < 2; i++)
                {
                    auto pWeaponStruct = pThis->GetWeapon(i);
                    if (!pWeaponStruct || !pWeaponStruct->WeaponType)
                        continue;

                    WeaponTypeClass* pWeapon = pWeaponStruct->WeaponType;
                    double maxHealth = GetCanTargetMaxHealth(pWeapon);
                    double minHealth = GetCanTargetMinHealth(pWeapon);

                    if (maxHealth < 1.0 || minHealth > 0.0)
                    {
                        double hp = pTarget->GetHealthPercentage();
                        if (hp >= maxHealth || hp < minHealth)
                            return 0; // target health outside threshold, skip
                    }

                    // First weapon with a health filter we found is authoritative
                    break;
                }
            }

            return 0x6F88BF;
        }
    }
    return 0;
}
