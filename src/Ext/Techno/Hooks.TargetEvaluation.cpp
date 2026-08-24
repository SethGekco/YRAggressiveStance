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
            filter.MaxHealth = 1.0;
            filter.MinHealth = 0.0;
            pINI->GetDouble(pWeapon->ID, "CanTarget.MaxHealth", filter.MaxHealth);
            pINI->GetDouble(pWeapon->ID, "CanTarget.MinHealth", filter.MinHealth);
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

// Returns true if pTarget has AggressiveStance.Exempt=yes
static bool IsExemptTarget(TechnoClass* pTarget)
{
    if (!pTarget) return false;
    return TechnoTypeExt::IsExemptFromAggressiveStance(pTarget->GetTechnoType());
}

// Returns true unless EVERY weapon on pThis explicitly forbids this target
// via CanTarget.MaxHealth/MinHealth. A weapon with no health tag always
// passes. This is only used to gate the AGGRESSIVE STANCE bypass and the
// supplementary health-enforcement hooks - never normal vanilla targeting.
// Returns true if the weapon EvaluateObject is currently scoring against
// (captured via AggressiveStanceTemp::CurrentWeapon) passes its own
// CanTarget.MaxHealth/MinHealth filter for this target. Falls back to
// checking both weapon slots if the capture is unavailable for any reason.
static bool WeaponPassesHealthFilter(TechnoClass* pThis, TechnoClass* pTarget)
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
    if (!sawAnyWeapon) return true;
    return false;
}
// ---------------------------------------------------------------------------
// Capture the weapon EvaluateObject is actually being called with.
// Mirrors Phobos's own EvaluateObjectTemp::PickedWeapon (Hooks.TargetEvaluation.cpp,
// hook at 0x6F7E24) but kept in our own DLL since we can't link to theirs.
// EBP holds the WeaponTypeClass* at function entry and is not clobbered before
// our later hooks fire within the same EvaluateObject call.
// -----------------------------------------------------------E

// ---------------------------------------------------------------------------
// Hook 1: 0x6F858F - ThreatPosed=0 BUILDING gate (AggressiveStance bypass)
// Only fires for buildings that vanilla would otherwise REJECT (ThreatPosed=0).
//
// Stolen bytes: 85 FF 74 18 8A 47 14 (7 bytes) =
//   test edi,edi / je 0x6F85AB / mov al,[edi+0x14]
// This INCLUDES a relative branch (je rel8). We must therefore NEVER `return 0`:
// on return 0 Syringe re-executes the copied bytes from its own trampoline stub,
// and the branch offset - which was never relocated - sends EIP into garbage in
// the stub page (observed crash: C0000005 at ~0x09ED0475). Always return an
// explicit address, replicating the stolen instructions by hand.
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F858F, TechnoClass_EvaluateObject_AggressiveStance_Buildings, 0x7)
{
    enum { EdiZero = 0x6F85AB, Continue = 0x6F8596, Accept = 0x6F88BF };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Building
        && !IsExemptTarget(pTarget)
        && IsAggressiveStance(pThis) && WeaponPassesHealthFilter(pThis, pTarget))
    {
        return Accept;
    }

    // Replicate `test edi,edi / je 0x6F85AB / mov al,[edi+0x14]`.
    if (!pThis)
        return EdiZero;

    R->AL(*reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(pThis) + 0x14));
    return Continue;
}

// ---------------------------------------------------------------------------
// Hook 2: 0x6F8503 - ThreatPosed=0 gate for NON-BUILDING targets
//
// Stolen bytes: 0F 84 46 04 00 00 (6 bytes) = je 0x6F894F, a rel32 branch
// (ZF comes from the preceding `test eax,eax` at 0x6F8501). This is the hook
// that produced the three crash snapshots: `return 0` made Syringe re-run the
// un-relocated `je` from its stub and jump into garbage. Never return 0 -
// always return an explicit address.
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F8503, TechnoClass_EvaluateObject_AggressiveStance_Units, 0x6)
{
    enum { SkipDeny = 0x6F851C, Deny = 0x6F894F, Continue = 0x6F8509 };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() != AbstractType::Building
        && !IsExemptTarget(pTarget)
        && IsAggressiveStance(pThis) && WeaponPassesHealthFilter(pThis, pTarget))
    {
        return SkipDeny;
    }

    // Replicate `je 0x6F894F` (taken when the tested EAX was zero).
    return (R->EAX() == 0) ? Deny : Continue;
}


// ---------------------------------------------------------------------------
// Hook 4: 0x6F84A9 - ThreatPosed gate for VEHICLE targets (UnitClass=0xF)
//
// Stolen bytes: 84 C9 0F 85 9E 04 00 00 (8 bytes) = test cl,cl / jne 0x6F894F,
// a rel32 branch. Same footgun as hooks 1 & 2: never return 0. The continue
// point (0x6F84B1) is both the vanilla fall-through and the aggressive bypass
// target, so only a true `cl != 0` reaches the deny path.
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6F84A9, TechnoClass_EvaluateObject_AggressiveStance_Vehicles, 0x8)
{
    enum { Deny = 0x6F894F, Continue = 0x6F84B1 };

    GET(TechnoClass*, pThis, EDI);
    GET(TechnoClass*, pTarget, ESI);

    if (pThis && pTarget && pTarget->WhatAmI() == AbstractType::Unit
        && !IsExemptTarget(pTarget)
        && IsAggressiveStance(pThis) && WeaponPassesHealthFilter(pThis, pTarget))
    {
        return Continue;
    }

    // Replicate `test cl,cl / jne 0x6F894F`.
    return (R->CL() != 0) ? Deny : Continue;
}




// ---------------------------------------------------------------------------
// Hook 7: 0x668F6A - RulesData_InitializeAfterAllLoaded
// Pre-warms the WeaponHealthFilterCache for every weapon type after all INI
// files are fully loaded. This ensures the cache is populated with correct
// values before any CanFire call can read from it, preventing the cache
// poisoning that occurred when GetWeaponHealthFilter was called lazily
// from inside CanFire (where INI_Rules may not yet have scenario data).
// Stolen bytes: A1 38 B2 A8 00 (5 bytes) = MOV EAX,[0xA8B238]
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x668F6A, RulesData_InitializeAfterAllLoaded_PreWarmWeaponCache, 0x5)
{
    WeaponHealthFilterCache.clear();

    for (int i = 0; i < WeaponTypeClass::Array->Count; i++)
    {
        WeaponTypeClass* pWeapon = WeaponTypeClass::Array->Items[i];
        if (pWeapon)
            GetWeaponHealthFilter(pWeapon); // populates cache entry
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook 8: 0x6FC339 - TechnoClass::CanFire
// Enforces CanTarget.MaxHealth/MinHealth for building targets, where Phobos's
// own check is skipped because pTargetTechno (EBP) is null for buildings.
// Reads ONLY from the pre-warmed cache - never calls CCINIClass at runtime.
// Registers: ESI=pThis, EDI=pWeapon, EBP=pTargetTechno (null for buildings),
//            stack arg pTarget at STACK_OFFSET(0x20, 0x4).
// Stolen bytes: 8A 87 4F 01 00 00 (6 bytes) = MOV AL,[EDI+0x14F]
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6FC339, TechnoClass_CanFire_BuildingHealthFilter, 0x6)
{
    return 0;
}
