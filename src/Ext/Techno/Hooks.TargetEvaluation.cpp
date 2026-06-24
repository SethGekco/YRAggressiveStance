#include <HouseClass.h>
#include <TechnoClass.h>
#include <WeaponTypeClass.h>
#include <WarheadTypeClass.h>
#include <Commands/AggressiveStance.h>
#include <Ext/TechnoType/Body.h>
#include <Helpers/Macro.h>
#include <CCINIClass.h>

// Returns the best weapon index (0=primary, 1=secondary) that can target pTarget,
// or -1 if neither can. Used to look up CanTarget.MaxHealth for the firing weapon.
static int GetBestWeaponIndex(TechnoClass* pThis, TechnoClass* pTarget)
{
    if (!pThis || !pTarget) return -1;
    auto pType = pThis->GetTechnoType();
    if (!pType) return -1;
    if (pType->Weapon[0].WeaponType) return 0;
    if (pType->Weapon[1].WeaponType) return 1;
    return -1;
}

// Read CanTarget.MaxHealth from Phobos INI for a given weapon type.
// Returns 1.0 (no restriction) if the tag is absent.
static float GetCanTargetMaxHealth(WeaponTypeClass* pWeapon)
{
    if (!pWeapon || !pWeapon->ID) return 1.0f;
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (!pINI) return 1.0f;
    // ReadFloat(section, key, default)
    double val = pINI->ReadDouble(pWeapon->ID, "CanTarget.MaxHealth", 1.0);
    return static_cast<float>(val);
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
            // Honor CanTarget.MaxHealth - check against primary weapon
            auto pType = pThis->GetTechnoType();
            if (pType)
            {
                WeaponTypeClass* pWeapon = nullptr;
                if (pType->Weapon[0].WeaponType)
                    pWeapon = pType->Weapon[0].WeaponType;
                else if (pType->Weapon[1].WeaponType)
                    pWeapon = pType->Weapon[1].WeaponType;

                if (pWeapon)
                {
                    float maxHealth = GetCanTargetMaxHealth(pWeapon);
                    if (maxHealth < 1.0f)
                    {
                        // Check target health ratio
                        int maxHP = pTarget->GetTechnoType()
                            ? pTarget->GetTechnoType()->Strength : 1;
                        if (maxHP > 0)
                        {
                            float healthRatio = static_cast<float>(pTarget->Health)
                                / static_cast<float>(maxHP);
                            if (healthRatio >= maxHealth)
                                return 0; // target too healthy, skip
                        }
                    }
                }
            }

            return 0x6F88BF;
        }
    }
    return 0;
}
