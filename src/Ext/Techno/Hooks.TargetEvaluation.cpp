#include <HouseClass.h>
#include <TechnoClass.h>
#include <Commands/AggressiveStance.h>
#include <Ext/TechnoType/Body.h>
#include <Helpers/Macro.h>

DEFINE_HOOK(0x6F858F, TechnoClass_EvaluateObject_AggressiveStance, 0x7)
{
    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pThis->Owner->IsControlledByHuman()
        && pTarget && pTarget->WhatAmI() == AbstractType::Building)
    {
        // Only apply aggressive stance bypass for ENEMY buildings.
        // Friendly/allied buildings must fall through to the vanilla
        // healing health check at 0x6F7F51 - bypassing it causes the
        // repair weapon to fire on full-health friendly buildings.
        if (pThis->Owner->IsAlliedWith(pTarget->Owner))
            return 0;

        bool isAggressive = AggressiveStanceClass::AggressiveStanceMap[pThis]
            || TechnoTypeExt::IsAlwaysAggressiveStance(pThis->GetTechnoType())
            || (pThis->Transporter && (AggressiveStanceClass::AggressiveStanceMap[pThis->Transporter]
                || TechnoTypeExt::IsAlwaysAggressiveStance(pThis->Transporter->GetTechnoType())));

        if (isAggressive)
            return 0x6F88BF;
    }
    return 0;
}
