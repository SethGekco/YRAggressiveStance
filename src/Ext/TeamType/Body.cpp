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
// ECX = TeamClass* (thiscall), stack arg 1 = FootClass* pFoot
// Prologue: 51 55 8B 6C 24 0C = push ecx; push ebp; mov ebp,[esp+0xC]
//           -> 6 stolen bytes land on an instruction boundary (OK).
//
// IMPORTANT: the engine calls this during team teardown with a FREED/garbage
// `this` (ECX) whose memory has already been reused (observed crash: ECX =
// 0x42555100, reused string bytes; C0000005 reading pTeam->Type). A non-null
// check does NOT prove ECX is a live TeamClass, so pTeam must not be
// dereferenced here. We don't need it: a unit leaving a team should drop its
// team-granted stance regardless of which team it was, and the only thing that
// must survive is the type-side AggressiveStance.Always. pFoot is valid (the
// engine dereferences it two instructions later). Erasing the map entry (rather
// than setting false) also avoids default-inserting a stale key.
// ---------------------------------------------------------------------------

#include <Ext/TechnoType/Body.h>

DEFINE_HOOK(0x6EA870, TeamClass_LiberateMember_AggressiveStance, 0x6)
{
    GET_STACK(FootClass*, pFoot, 0x4);

    if (pFoot && !TechnoTypeExt::IsAlwaysAggressiveStance(pFoot->GetTechnoType()))
        AggressiveStanceClass::AggressiveStanceMap.erase(pFoot);

    return 0;
}
