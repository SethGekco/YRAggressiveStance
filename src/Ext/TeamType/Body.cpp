#include "Body.h"

#include <Helpers/Macro.h>
#include <TeamClass.h>
#include <TeamTypeClass.h>
#include <FootClass.h>
#include <CCINIClass.h>
#include <Commands/AggressiveStance.h>

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

std::map<TeamTypeClass*, bool> TeamTypeExt::AggressiveStanceMap;

bool TeamTypeExt::IsAggressiveStance(TeamTypeClass* pType)
{
    if (!pType) return false;

    auto it = TeamTypeExt::AggressiveStanceMap.find(pType);
    if (it != TeamTypeExt::AggressiveStanceMap.end())
        return it->second;

    // Lazy read from INI on first encounter, then cache.
    CCINIClass* pINI = CCINIClass::INI_Rules;
    if (pINI && pType->ID)
    {
        bool val = pINI->ReadBool(pType->ID, "AggressiveStance", false);
        TeamTypeExt::AggressiveStanceMap[pType] = val;
        return val;
    }

    TeamTypeExt::AggressiveStanceMap[pType] = false;
    return false;
}

// ---------------------------------------------------------------------------
// Hook: TeamClass::AddMember (0x6EA500)
// Called when a FootClass unit is assigned to a team.
// If the team type has AggressiveStance=yes, seed AggressiveStanceMap for
// the unit so our EvaluateObject hooks treat it as aggressive.
// ECX = TeamClass* (thiscall), stack arg 1 = FootClass* pFoot
// Prologue: 53 56 57 8B 7C 24 10 = push ebx; push esi; push edi;
//           mov edi,[esp+0x10]   -> must steal 7 bytes (6 cuts the mov,
//           leaving 0x10 to execute as a stray opcode and crash the game).
// ---------------------------------------------------------------------------

DEFINE_HOOK(0x6EA500, TeamClass_AddMember_AggressiveStance, 0x7)
{
    GET(TeamClass*,  pTeam, ECX);
    GET_STACK(FootClass*, pFoot, 0x4);

    if (pTeam && pFoot && pTeam->Type)
    {
        if (TeamTypeExt::IsAggressiveStance(pTeam->Type))
        {
            AggressiveStanceClass::AggressiveStanceMap[pFoot] = true;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Hook: TeamClass::LiberateMember (0x6EA870)
// Called when a unit leaves or is removed from a team.
// Clear the aggressive stance map entry ONLY if the unit's stance came from
// the team (i.e. the unit's TechnoType doesn't have AggressiveStance.Always).
// ECX = TeamClass* (thiscall), stack arg 1 = FootClass* pFoot
// Prologue: 51 55 8B 6C 24 0C = push ecx; push ebp; mov ebp,[esp+0xC]
//           -> 6 stolen bytes land on an instruction boundary (OK).
// ---------------------------------------------------------------------------

#include <Ext/TechnoType/Body.h>

DEFINE_HOOK(0x6EA870, TeamClass_LiberateMember_AggressiveStance, 0x6)
{
    GET(TeamClass*,  pTeam, ECX);
    GET_STACK(FootClass*, pFoot, 0x4);

    if (pTeam && pFoot && pTeam->Type)
    {
        if (TeamTypeExt::IsAggressiveStance(pTeam->Type))
        {
            // Don't clear if the unit has AggressiveStance.Always=yes on its type —
            // that flag is independent of team membership.
            TechnoTypeClass* pTechnoType = pFoot->GetTechnoType();
            if (!TechnoTypeExt::IsAlwaysAggressiveStance(pTechnoType))
            {
                AggressiveStanceClass::AggressiveStanceMap[pFoot] = false;
            }
        }
    }

    return 0;
}
