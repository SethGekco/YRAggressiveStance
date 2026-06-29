#pragma once

#include <TeamTypeClass.h>
#include <map>

// Extends TeamTypeClass with data read from INI that cannot be stored in the
// fixed vanilla struct.
class TeamTypeExt
{
public:
    // True if units belonging to a team of this type should behave as if they
    // are in Aggressive Stance while they remain members of the team.
    // Corresponds to the INI tag on [SomeTeamType]:
    //   AggressiveStance=yes   (default: no)
    static std::map<TeamTypeClass*, bool> AggressiveStanceMap;

    // Returns true if pType has AggressiveStance=yes.
    static bool IsAggressiveStance(TeamTypeClass* pType);
};
