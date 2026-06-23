#include "Body.h"

#include <Helpers/Macro.h>
#include <TechnoTypeClass.h>
#include <TechnoClass.h>
#include <CCINIClass.h>
#include <Utilities/Parser.h>

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

std::map<TechnoTypeClass*, bool> TechnoTypeExt::AggressiveStanceAlwaysMap;

bool TechnoTypeExt::IsAlwaysAggressiveStance(TechnoTypeClass* pType)
{
    auto it = AggressiveStanceAlwaysMap.find(pType);
    return (it != AggressiveStanceAlwaysMap.end()) && it->second;
}

// ---------------------------------------------------------------------------
// Hook: TechnoTypeClass::LoadFromINI  (0x7162A0)
//
// This virtual is called once per TechnoType section when rules/art INI files
// are loaded.  EDI holds the TechnoTypeClass* and ESI holds the CCINIClass*.
// We append our own read after the vanilla processing completes.
//
// Size stolen: 0x5 bytes (PUSH EBP; MOV EBP,ESP at function entry –
// the hook jumps in at the very top and lets Syringe put the trampoline back).
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x7162A0, TechnoTypeClass_LoadFromINI_AggressiveStanceAlways, 0x5)
{
    GET(TechnoTypeClass*, pThis, EDI);
    GET(CCINIClass*,      pINI,  ESI);

    const char* pSection = pThis->ID;

    bool value = false;
    // ReadBool(section, key, default) – returns the default when the key is absent,
    // so this is safe for types that never set the tag.
    value = pINI->ReadBool(pSection, "AggressiveStance.Always", false);

    if (value)
    {
        TechnoTypeExt::AggressiveStanceAlwaysMap[pThis] = true;
    }
    // No need to store false entries – absence from the map is treated as false.

    return 0;   // continue into the vanilla function body
}

// ---------------------------------------------------------------------------
// Hook: TechnoClass::Init  (0x6F42F0)
//
// Called when a TechnoClass instance is placed on the map for the first time.
// EDI holds the TechnoClass*.  If its type has AggressiveStance.Always=yes,
// we pre-populate AggressiveStanceMap so the target-evaluation hook fires
// immediately without any player input.
//
// We must include AggressiveStance.h here because we write into
// AggressiveStanceClass::AggressiveStanceMap directly.
// ---------------------------------------------------------------------------

#include <Commands/AggressiveStance.h>

DEFINE_HOOK(0x6F42F0, TechnoClass_Init_AggressiveStanceAlways, 0x6)
{
    GET(TechnoClass*, pThis, ESI);

    if (pThis)
    {
        TechnoTypeClass* pType = pThis->GetTechnoType();
        if (pType && TechnoTypeExt::IsAlwaysAggressiveStance(pType))
        {
            AggressiveStanceClass::AggressiveStanceMap[pThis] = true;
        }
    }

    return 0;   // continue into vanilla Init body
}
